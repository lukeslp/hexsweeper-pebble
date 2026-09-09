// Hexsweeper Sphere. Luke Steuber.
#include "compat.h"
#include "controller.h"
#include <math.h>
#include <pebble.h>
static int round_coord(float x) { return (int)(x < 0 ? x - .5f : x + .5f); }
static Controller s;
static Window *window;
static Layer *canvas;
static AppTimer *frame_timer;
static AppTimer *tap_timer;
static uint32_t gesture_clock;
static uint32_t last_frame;
static bool focused = true, save_failed;
static bool touch_ready;
static int width, height;
enum { SAVE_KEY = 1 };
_Static_assert(sizeof(SavedGame) <= PERSIST_DATA_MAX_LENGTH,
               "Save exceeds one atomic record");
static uint32_t now_ms(void) {
  time_t seconds;
  uint16_t ms;
  time_ms(&seconds, &ms);
  return (uint32_t)seconds * 1000u + ms;
}
static void record(void) {
  SavedGame save;
  controller_save(&s, &save);
  save_failed =
      persist_write_data(SAVE_KEY, &save, sizeof save) != (int)sizeof save;
}
static void trace(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "HS seed=%lu flags=%d",
          (unsigned long)s.game.seed, game_flag_count(&s.game));
  APP_LOG(APP_LOG_LEVEL_INFO,
          "HS mode=%u selected=%u neighbor=%u menu=%u help=%u direct=%u "
          "tool=%u status=%u opened=%d",
          s.mode, s.selected, controller_highlight(&s), s.menu, s.help,
          s.direct, s.flag, s.game.status, game_open_count(&s.game));
}
static void redraw(void) {
  if (canvas)
    layer_mark_dirty(canvas);
}
static void cancel_frame(void) {
  if (frame_timer) {
    app_timer_cancel(frame_timer);
    frame_timer = NULL;
  }
}
static void cancel_tap(void) {
  if (tap_timer) {
    app_timer_cancel(tap_timer);
    tap_timer = NULL;
  }
}
static void expire_tap(void *ctx) {
  tap_timer = NULL;
  gesture_clock = 351;
}
static void frame(void *ctx) {
  frame_timer = NULL;
  if (focused)
    redraw();
}
static void schedule(void) {
  if (!frame_timer) {
    uint32_t elapsed = now_ms() - last_frame;
    if (elapsed > 1000)
      elapsed = 0; // Wall-clock corrections cannot rush a frame.
    frame_timer =
        app_timer_register(elapsed >= 50 ? 1 : 50 - elapsed, frame, NULL);
  }
}
static void text(GContext *ctx, const char *value, GRect rect, GFont font,
                 GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, value, font, rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}
static void flag_symbol(GContext *ctx, int x, int y, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(x - 4, y - 6), GPoint(x - 4, y + 7));
  graphics_draw_line(ctx, GPoint(x - 7, y + 7), GPoint(x + 3, y + 7));
  GPoint points[] = {GPoint(x - 3, y - 6), GPoint(x + 6, y - 2),
                     GPoint(x - 3, y + 1)};
  GPath p = {.num_points = 3, .points = points};
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, &p);
}
static void mine_symbol(GContext *ctx, int x, int y) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, GPoint(x, y), 4);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(x - 7, y), GPoint(x + 7, y));
  graphics_draw_line(ctx, GPoint(x, y - 7), GPoint(x, y + 7));
}
static void board(GContext *ctx) {
  Polygon poly;
  GPoint points[8];
  GPath path = {.points = points};
  int order[HS_CELLS];
  float depth[HS_CELLS];
  for (int i = 0; i < HS_CELLS; i++) {
    order[i] = i;
    depth[i] = sphere_rotate(s.orientation, sphere_center(i)).z;
  }
  for (int i = 1; i < HS_CELLS; i++) {
    int cell = order[i], j = i;
    while (j > 0 && depth[order[j - 1]] > depth[cell]) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = cell;
  }
  int highlight = controller_highlight(&s);
  for (int pass = 0; pass < 2; pass++)
    for (int k = 0; k < HS_CELLS; k++) {
      int i = order[k];
      if (pass == 1 && i != highlight)
        continue;
      sphere_polygon(s.orientation, s.viewport, i, &poly);
      if (poly.count < 3)
        continue;
      for (int j = 0; j < poly.count; j++)
        points[j] = GPoint((int16_t)round_coord(poly.p[j].x),
                           (int16_t)round_coord(poly.p[j].y));
      path.num_points = (uint32_t)poly.count;
      bool open = (s.game.cells[i] & HS_OPEN) != 0;
      bool mine = (s.game.cells[i] & HS_MINE) != 0 && s.game.status == HS_LOST;
      GColor fill = mine ? GColorMelon : open ? GColorWhite : GColorDarkGray;
      if (pass == 0) {
        graphics_context_set_fill_color(ctx, fill);
        gpath_draw_filled(ctx, &path);
      }
      graphics_context_set_stroke_color(ctx, pass == 1 ? GColorChromeYellow
                                                       : GColorLightGray);
      graphics_context_set_stroke_width(ctx, pass == 1 ? 3 : 1);
      gpath_draw_outline(ctx, &path);
      if (pass == 0 && !open && !mine && poly.center.z >= .6f) {
        // Inset polygon gives covered cells a raised shape independent of hue.
        int cx = round_coord(s.viewport.x + s.viewport.r * poly.center.x);
        int cy = round_coord(s.viewport.y - s.viewport.r * poly.center.y);
        GPoint inset[8];
        for (int j = 0; j < poly.count; j++)
          inset[j] = GPoint(cx + (points[j].x - cx) * 3 / 4,
                            cy + (points[j].y - cy) * 3 / 4);
        GPath rim = {.num_points = poly.count, .points = inset};
        graphics_context_set_stroke_color(ctx, GColorLightGray);
        gpath_draw_outline(ctx, &rim);
      }
      if (pass == 1 || poly.center.z < .6f)
        continue;
      int x = (int)round_coord(s.viewport.x + s.viewport.r * poly.center.x),
          y = (int)round_coord(s.viewport.y - s.viewport.r * poly.center.y);
      if (mine)
        mine_symbol(ctx, x, y);
      else if (s.game.cells[i] & HS_FLAG)
        flag_symbol(ctx, x, y, GColorChromeYellow);
      else if (!open) {
        // Sparse covered-cell texture; projected rim facets stay uncluttered.
        graphics_context_set_stroke_color(ctx, GColorLightGray);
        graphics_context_set_stroke_width(ctx, 1);
        for (int dx = -4; dx <= 4; dx += 4)
          for (int dy = -4; dy <= 4; dy += 4)
            graphics_draw_pixel(ctx, GPoint(x + dx, y + dy));
      } else if (open) {
        int count = game_clue(&s.game, i);
        if (count) {
          char n[2] = {(char)('0' + count), 0};
          text(ctx, n, GRect(x - 13, y - 16, 26, 30),
               fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GColorBlack);
        }
      }
    }
  graphics_context_set_stroke_width(ctx, 1);
  char top[48];
  snprintf(top, sizeof top, "%d/%d safe  %d flags", game_open_count(&s.game),
           HS_CELLS - HS_MINES, game_flag_count(&s.game));
  int top_inset = PBL_IF_ROUND_ELSE(62, 4);
  text(ctx, top,
       GRect(top_inset, PBL_IF_ROUND_ELSE(13, 0), width - 2 * top_inset, 20),
       fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorWhite);
  char bottom[50];
  int c = highlight;
  const char *tool = s.flag ? "FLAG" : "DIG";
  if (s.game.status == HS_WON)
    snprintf(bottom, sizeof bottom, "CLEARED · Back: menu");
  else if (s.game.status == HS_LOST)
    snprintf(bottom, sizeof bottom, "MINE · Back: menu");
  else if (s.mode == HS_EXPLORE)
    snprintf(bottom, sizeof bottom, "EXPLORE  %u/%u", s.neighbor + 1,
             s.neighbor_count);
  else if (s.game.cells[c] & HS_FLAG)
    snprintf(bottom, sizeof bottom, "%s · Flagged", tool);
  else if (s.game.cells[c] & HS_OPEN)
    snprintf(bottom, sizeof bottom, "%s · %u nearby", tool,
             game_clue(&s.game, c));
  else
    snprintf(bottom, sizeof bottom, "%s · Covered", tool);
  int inset = PBL_IF_ROUND_ELSE(55, 3);
  text(ctx, bottom, GRect(inset, height - 40, width - 2 * inset, 23),
       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GColorWhite);
  const char *hint = s.mode == HS_EXPLORE ? "Select: move  Back: play"
                     : s.direct           ? "Tap acts · Up: explore"
                                          : "Tap aims · Select: act";
  if (!touch_ready && s.mode == HS_PLAY)
    hint = "Up: explore · Select: act";
  if (save_failed)
    hint = "Save failed · Keep app open";
  text(ctx, hint,
       GRect(PBL_IF_ROUND_ELSE(64, 3), height - 21,
             width - PBL_IF_ROUND_ELSE(128, 6), 18),
       fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorLightGray);
}
static void menu(GContext *ctx) {
  const int inset = PBL_IF_ROUND_ELSE(43, 14), w = width - 2 * inset;
  const char *title = s.mode == HS_CONFIRM ? "NEW SPHERE?"
                      : s.mode == HS_HELP  ? "HOW TO PLAY"
                                           : "HEXSWEEPER";
  text(ctx, title, GRect(inset, PBL_IF_ROUND_ELSE(25, 15), w, 27),
       fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GColorWhite);
  if (s.mode == HS_HELP) {
    const char *pages[] = {
        "42 cells. Seven mines.\nNumbers count nearby mines.\nThe first dig "
        "opens a safe patch.",
        "Drag to rotate. Tap to aim.\nSelect digs. Hold it to flag.\nDown "
        "switches Dig / Flag.",
        "Up opens Explore.\nUp / Down choose a neighbor.\nSelect moves. Back "
        "returns to play.",
        "Back opens the menu.\nChoose direct taps there.\nRim taps only "
        "aim.\nGames save on this watch."};
    if (!touch_ready) {
      pages[1] = "Select digs. Hold it to flag.\nDown switches Dig / Flag.\nUp "
                 "opens Explore.";
      pages[3] = "Touch needs firmware 4.33.2 or later.\nUse buttons to "
                 "play.\nGames save on this watch.";
    }
    text(ctx, pages[s.help], GRect(inset, 64, w, height - 106),
         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GColorWhite);
    char hint[36];
    snprintf(hint, sizeof hint, "%u/4  Up/Down · Select: play", s.help + 1);
    text(ctx, hint, GRect(inset, height - 40, w, 30),
         fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorLightGray);
    return;
  }
  const char *items[5] = {"Resume",
                          s.direct ? "Touch: direct" : "Touch: aim first",
                          "Controls", "New game", "Save & exit"};
  if (!touch_ready)
    items[1] = "Touch: update watch";
  const char *confirm[2] = {"Keep playing", "Replace game"};
  int count = s.mode == HS_CONFIRM ? 2 : 5;
  if (s.mode == HS_CONFIRM)
    text(ctx, "Your current board will be replaced.", GRect(inset, 65, w, 48),
         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GColorWhite);
  int start = s.mode == HS_CONFIRM ? 125 : 60, row = 27;
  for (int i = 0; i < count; i++) {
    GRect rect = GRect(inset, start + i * row, w, row);
    bool selected = i == s.menu;
    if (selected) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, rect, 3, GCornersAll);
    }
    text(ctx, s.mode == HS_CONFIRM ? confirm[i] : items[i], rect,
         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
         selected ? GColorBlack : GColorWhite);
  }
  if (save_failed)
    text(ctx, "Save failed. Back: play", GRect(inset, height - 27, w, 20),
         fonts_get_system_font(FONT_KEY_GOTHIC_14), GColorWhite);
}
static void draw(Layer *layer, GContext *ctx) {
  uint32_t start = now_ms();
  last_frame = start;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornersAll);
  graphics_context_set_antialiased(ctx, true);
  if (s.mode == HS_PLAY || s.mode == HS_EXPLORE)
    board(ctx);
  else
    menu(ctx);
  uint32_t elapsed = now_ms() - start;
  if (s.gesture.drag && elapsed < 1000)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "HS frame_ms=%lu", (unsigned long)elapsed);
}
static void button(int b) {
  if (!touch_ready && s.mode == HS_PAUSE && s.menu == 1 && b == HS_SELECT) {
    s.mode = HS_HELP;
    s.help = 3;
    redraw();
    trace();
    return;
  }
  bool exit = s.mode == HS_PAUSE && s.menu == 4 && b == HS_SELECT;
  controller_button(&s, b);
  if (!s.gesture.active)
    cancel_tap();
  cancel_frame();
  record();
  trace();
  redraw();
  if (exit && !save_failed)
    window_stack_pop_all(false);
}
static void up(ClickRecognizerRef r, void *ctx) { button(HS_UP); }
static void down(ClickRecognizerRef r, void *ctx) { button(HS_DOWN); }
static void select_click(ClickRecognizerRef r, void *ctx) { button(HS_SELECT); }
static void select_hold(ClickRecognizerRef r, void *ctx) {
  button(HS_HOLD_SELECT);
}
static void back(ClickRecognizerRef r, void *ctx) { button(HS_BACK); }
static void clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up);
  window_single_click_subscribe(BUTTON_ID_DOWN, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_hold, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
}
static void touch(const TouchEvent *e, void *ctx) {
  if (!focused)
    return;
  int type = e->type == TouchEvent_Touchdown ? HS_TOUCH_DOWN
             : e->type == TouchEvent_Liftoff ? HS_TOUCH_UP
                                             : HS_TOUCH_MOVE;
  // AppTimer is independent of phone/RTC corrections. The controller only needs
  // to distinguish an eligible tap from one whose 350 ms deadline has expired.
  if (type == HS_TOUCH_DOWN) {
    cancel_tap();
    gesture_clock = 0;
  }
  controller_touch(&s, type, e->x, e->y, gesture_clock, e->non_navigational);
  if (type == HS_TOUCH_DOWN && s.gesture.active)
    tap_timer = app_timer_register(350, expire_tap, NULL);
  if (type == HS_TOUCH_UP || e->non_navigational) {
    cancel_tap();
    cancel_frame();
    record();
    trace();
    redraw();
  } else if (s.gesture.drag)
    schedule();
}
static void focus(bool active) {
  focused = active;
  controller_cancel(&s);
  cancel_tap();
  cancel_frame();
  record();
  if (active)
    redraw();
}
static void load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect bounds = layer_get_bounds(root);
  width = bounds.size.w;
  height = bounds.size.h;
  Viewport view = {width / 2.f,
                   PBL_IF_ROUND_ELSE(height / 2.f - 4, height / 2.f - 9),
                   PBL_IF_ROUND_ELSE(99, 86)};
  controller_init(&s, (uint32_t)time(NULL), view);
  SavedGame save;
  bool restored = false;
  if (persist_exists(SAVE_KEY)) {
    int n = persist_read_data(SAVE_KEY, &save, sizeof save);
    if (n > 0)
      restored = controller_restore(&s, &save, (size_t)n);
  }
  if (!restored)
    s.mode = HS_HELP;
  canvas = layer_create(bounds);
  layer_set_update_proc(canvas, draw);
  layer_add_child(root, canvas);
  trace();
}
static void unload(Window *w) {
  layer_destroy(canvas);
  canvas = NULL;
}
int main(void) {
  WatchInfoVersion firmware = watch_info_get_firmware_version();
  touch_ready =
      firmware_allows_touch(firmware.major, firmware.minor, firmware.patch);
  APP_LOG(APP_LOG_LEVEL_INFO, "HS firmware=%u.%u.%u touch=%u", firmware.major,
          firmware.minor, firmware.patch, touch_ready);
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_window_handlers(window,
                             (WindowHandlers){.load = load, .unload = unload});
  window_set_click_config_provider(window, clicks);
  // Third-party apps are opted out of system touch navigation by default.
  // Do not call the newer bridge API just to retain that default.
  window_stack_push(window, false);
  if (touch_ready)
    touch_service_subscribe(touch, NULL);
  app_focus_service_subscribe(focus);
  app_event_loop();
  controller_cancel(&s);
  cancel_tap();
  record();
  cancel_frame();
  if (touch_ready)
    touch_service_unsubscribe();
  app_focus_service_unsubscribe();
  window_destroy(window);
}
