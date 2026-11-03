// Hexsweeper 3D. Luke Steuber.
#include "compat.h"
#include "controller.h"
#include "flat.h"
#include "about.h"
#include "ui_style.h"
#include <math.h>
#include <pebble.h>
static int round_coord(float x) { return (int)(x < 0 ? x - .5f : x + .5f); }
static Controller s;
static Window *window;
static Layer *canvas;
static AppTimer *frame_timer;
static AppTimer *tap_timer;
static uint32_t last_frame;
static bool focused = true, save_failed;
static bool touch_ready, showing_about;
static int width, height;
static int app_mode = 2, mode_choice;
static bool picker, chose_mode, picker_touched;
#ifdef PBL_TOUCH
static uint32_t gesture_clock;
static bool picker_dragged;
static GPoint picker_down;
static uint32_t picker_time;
#endif
enum { SAVE_KEY = 1 };
// Only Time 2 and Round 2 have touchscreens. Elsewhere the pause menu hides
// its touch row and the touch handler is not compiled.
#ifdef PBL_TOUCH
#define HS_SHOWN_ROW(i) true
#else
#define HS_SHOWN_ROW(i) ((i) != 1)
#endif
_Static_assert(sizeof(SavedGame) <= PERSIST_DATA_MAX_LENGTH,
               "Save exceeds one atomic record");
static uint32_t now_ms(void) {
  time_t seconds;
  uint16_t ms;
  time_ms(&seconds, &ms);
  return (uint32_t)seconds * 1000u + ms;
}
static void record(void) {
  if (picker && !chose_mode)
    return;
  if (app_mode != 2) {
    save_failed = !flat_save();
    if (!save_failed)
      save_failed = persist_write_int(2, app_mode) != sizeof(int32_t);
    return;
  }
  SavedGame save;
  controller_save(&s, &save);
  save_failed =
      persist_write_data(SAVE_KEY, &save, sizeof save) != (int)sizeof save;
  if (!save_failed)
    save_failed = persist_write_int(2, app_mode) != sizeof(int32_t);
}
static void trace(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "HX app=%d picker=%d choice=%d about=%d", app_mode, picker,
          mode_choice, showing_about);
  if (app_mode != 2) {
    flat_trace();
    return;
  }
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
#ifdef PBL_TOUCH
static void expire_tap(void *ctx) {
  tap_timer = NULL;
  gesture_clock = 351;
}
#endif
static void frame(void *ctx) {
  frame_timer = NULL;
  if (focused) {
    if (!picker && app_mode != 2) {
      flat_tick();
      if (flat_busy())
        frame_timer = app_timer_register(25, frame, NULL);
    }
    redraw();
  }
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
static GRect inset_text(GRect r, int dy) {
  return GRect(r.origin.x, r.origin.y + dy, r.size.w, r.size.h);
}
static void flag_symbol(GContext *ctx, int x, int y, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(x - 5, y - 8), GPoint(x - 5, y + 8));
  graphics_draw_line(ctx, GPoint(x - 8, y + 8), GPoint(x + 4, y + 8));
  GPoint points[] = {GPoint(x - 4, y - 8), GPoint(x + 8, y - 3),
                     GPoint(x - 4, y + 2)};
  GPath p = {.num_points = 3, .points = points};
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, &p);
}
static void mine_symbol(GContext *ctx, int x, int y, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(x, y), 4);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(x - 7, y), GPoint(x + 7, y));
  graphics_draw_line(ctx, GPoint(x, y - 7), GPoint(x, y + 7));
}
// Not inlined: its large frame must not sit beneath the flat-board, menu and
// picker paths, which call firmware text layout from deeper frames.
static __attribute__((noinline)) void board(GContext *ctx) {
  const Layout *l = &ui_layout;
  Polygon poly;
  GPoint points[8];
  GPath path = {.points = points};
  // Rendering is synchronous; keep sorting scratch off the graphics call stack.
  static int order[HS_CELLS];
  static float depth[HS_CELLS];
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
      bool flagged = (s.game.cells[i] & HS_FLAG) != 0;
      bool open = (s.game.cells[i] & HS_OPEN) != 0;
      bool mine = (s.game.cells[i] & HS_MINE) != 0 && s.game.status == HS_LOST;
      bool blast = mine && open; // The one detonated mine is also open.
      (void)blast;
      GColor fill = mine      ? UI_MINE_FILL(blast)
                    : flagged ? UI_FLAG_FILL
                    : open    ? GColorWhite
                              : UI_COVERED;
      if (pass == 0) {
        graphics_context_set_fill_color(ctx, fill);
        gpath_draw_filled(ctx, &path);
      }
      // Focus uses cyan over a white border, independently of amber flags.
      if (pass == 1) {
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_stroke_width(ctx, 5);
        gpath_draw_outline(ctx, &path);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 3);
        gpath_draw_outline(ctx, &path);
      }
      graphics_context_set_stroke_color(ctx, pass == 1 ? UI_FOCUS_CORE
                                                       : ui_edge_for(fill));
      graphics_context_set_stroke_width(ctx, 1);
      gpath_draw_outline(ctx, &path);
      if (pass == 0 && !open && !mine && !flagged && poly.center.z >= .6f) {
        // Inset polygon gives covered cells a raised shape independent of hue.
        int cx = round_coord(s.viewport.x + s.viewport.r * poly.center.x);
        int cy = round_coord(s.viewport.y - s.viewport.r * poly.center.y);
        GPoint inset[8];
        for (int j = 0; j < poly.count; j++)
          inset[j] = GPoint(cx + (points[j].x - cx) * 3 / 4,
                            cy + (points[j].y - cy) * 3 / 4);
        GPath rim = {.num_points = poly.count, .points = inset};
        graphics_context_set_stroke_color(ctx, UI_EDGE);
        gpath_draw_outline(ctx, &rim);
      }
      if (pass == 1 || poly.center.z < .6f)
        continue;
      int x = (int)round_coord(s.viewport.x + s.viewport.r * poly.center.x),
          y = (int)round_coord(s.viewport.y - s.viewport.r * poly.center.y);
      if (mine)
        mine_symbol(ctx, x, y, UI_MINE_INK(blast));
      else if (s.game.cells[i] & HS_FLAG)
        flag_symbol(ctx, x, y, UI_FLAG_MARK);
      else if (!open) {
        // Sparse covered-cell texture; projected rim facets stay uncluttered.
        graphics_context_set_stroke_color(ctx, UI_EDGE);
        graphics_context_set_stroke_width(ctx, 1);
        for (int dx = -4; dx <= 4; dx += 4)
          for (int dy = -4; dy <= 4; dy += 4)
            graphics_draw_pixel(ctx, GPoint(x + dx, y + dy));
      } else if (open) {
        int count = game_clue(&s.game, i);
        if (count) {
          char n[2] = {(char)('0' + count), 0};
          text(ctx, n, GRect(x - 13, y - 16, 26, 30),
               fonts_get_system_font(UI_FONT_BODY), GColorBlack);
        }
      }
    }
  graphics_context_set_stroke_width(ctx, 1);
  // Rendering is synchronous; text buffers stay off the small watch stack.
  static char top[48];
  // The small round screen's top chord holds the short form.
  snprintf(top, sizeof top,
           l->hud == HUD_ROUND_SMALL ? "%d/%d · %d flags" : "%d/%d safe  %d flags",
           game_open_count(&s.game), HS_CELLS - HS_MINES, game_flag_count(&s.game));
  // Opaque readouts keep enlarged facets from running behind critical text.
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, width, l->bar_top), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(0, height - l->bar_bottom, width, l->bar_bottom), 0, GCornerNone);
  text(ctx, top, ui_rect(l->sphere_top), fonts_get_system_font(UI_FONT_SMALL), GColorWhite);
  static char bottom[50];
  int c = highlight;
  const char *tool = s.flag ? "FLAG" : "DIG";
  if (s.game.status == HS_WON)
    snprintf(bottom, sizeof bottom, "CLEARED");
  else if (s.game.status == HS_LOST)
    snprintf(bottom, sizeof bottom, "MINE HIT");
  else if (s.mode == HS_EXPLORE)
    snprintf(bottom, sizeof bottom, "MOVE %u/%u", s.neighbor + 1, s.neighbor_count);
  else if (l->hud == HUD_ROUND_SMALL)
    snprintf(bottom, sizeof bottom, "%s", tool); // The cell shows its own state.
  else if (s.game.cells[c] & HS_FLAG)
    snprintf(bottom, sizeof bottom, "%s · flagged", tool);
  else if (s.game.cells[c] & HS_OPEN)
    snprintf(bottom, sizeof bottom, "%s · %u nearby", tool, game_clue(&s.game, c));
  else
    snprintf(bottom, sizeof bottom, "%s · covered", tool);
  text(ctx, bottom, ui_rect(l->sphere_bottom), fonts_get_system_font(UI_FONT_BODY),
       GColorWhite);
}
// Pause rows on screen. Watches without a touchscreen omit the touch row (1).
static int pause_rows(uint8_t shown[6]) {
  int n = 0;
  for (int i = 0; i < 6; i++)
    if (HS_SHOWN_ROW(i))
      shown[n++] = (uint8_t)i;
  return n;
}
static int first_row(int position, int rows) {
  return position < rows ? 0 : position - rows + 1;
}
static __attribute__((noinline)) void menu(GContext *ctx) {
  const Layout *l = &ui_layout;
  const int inset = l->inset, w = width - 2 * inset;
  const char *title = s.mode == HS_CONFIRM ? "NEW SPHERE?"
                      : s.mode == HS_HELP  ? "HOW TO PLAY"
                      : l->hud == HUD_FULL ? "HEXSWEEPER 3D"
                                           : "3D";
  text(ctx, title,
       GRect(inset, s.mode == HS_CONFIRM ? l->confirm_title_y : l->title_y, w, l->title_h),
       ui_font(l->title_font), GColorWhite);
  if (s.mode == HS_HELP) {
#ifdef PBL_TOUCH
    const char *pages[] = {
        "42 cells. 7 mines.\nFirst dig is safe.\nSelect open clues:\nmatching flags open\ntheir neighbors.",
        "Drag to turn.\nTap to aim.\nSelect digs.\nHold Select: flag.\nDown: Dig / Flag.",
        "Up: explore.\nUp/Down: choose\na neighbor.\nSelect: move there.\nBack: play.",
        "Back: menu.\nDirect taps are\noptional there.\nRim taps only aim.\nBoards stay saved."};
    if (!touch_ready) {
      pages[1] = "Select digs. Hold it to flag.\nDown switches Dig / Flag.\nUp "
                 "opens Explore.";
      pages[3] = "Touch needs\nfirmware 4.33.2.\nButtons still play.\nBoards stay saved.";
    }
#else
    // Button-only watches: no touch wording, and lines short enough for 144 px.
    const char *pages[] = {
        "42 cells. 7 mines.\nFirst dig is safe.\nSelect open clues:\nmatching flags\nopen neighbors.",
        "Select digs.\nHold Select: flag.\nDown: Dig / Flag.\nUp: explore around\nthe sphere.",
        "Up: explore.\nUp/Down: choose\na neighbor.\nSelect: move there.\nBack: play.",
        "Back: menu.\nNew game asks\nbefore replacing.\nBoards stay saved."};
#endif
    text(ctx, pages[s.help], GRect(inset, l->help_body_y, w, l->help_body_h),
         ui_font(l->help_font), GColorWhite);
    char hint[36];
    snprintf(hint, sizeof hint, "%u/4  Select: play", s.help + 1);
    text(ctx, hint, GRect(inset, l->help_hint_y, w, 26),
         fonts_get_system_font(UI_FONT_SMALL), GColorWhite);
    return;
  }
  const char *items[6] = {"Resume",
                          s.direct ? "Direct taps: on" : "Direct taps: off",
                          "Controls", "New game", "Game modes", "About"};
  if (!touch_ready)
    items[1] = "Touch: update watch";
  const char *confirm[2] = {"Keep playing", "Replace game"};
  uint8_t shown[6] = {0, 1};
  int count = s.mode == HS_CONFIRM ? 2 : pause_rows(shown);
  if (s.mode == HS_CONFIRM)
    text(ctx, "Replace this board?", GRect(inset, l->confirm_body_y, w, l->confirm_body_h),
         ui_font(l->confirm_body_font), GColorWhite);
  int position = 0;
  while (position < count - 1 && shown[position] != s.menu)
    position++;
  int first = s.mode == HS_CONFIRM ? 0 : first_row(position, l->rows);
  int start = s.mode == HS_CONFIRM ? l->confirm_top : l->menu_top;
  for (int k = first; k < count && k < first + l->rows; k++) {
    int i = shown[k];
    GRect rect = GRect(inset, start + (k - first) * l->row_h, w, l->row_h);
    bool selected = i == s.menu;
    if (selected) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, rect, 3, GCornersAll);
    }
    text(ctx, s.mode == HS_CONFIRM ? confirm[i] : items[i], inset_text(rect, l->row_dy),
         fonts_get_system_font(UI_FONT_BODY),
         selected ? GColorBlack : GColorWhite);
  }
  if (save_failed)
    text(ctx, "Save failed. Back: play", ui_rect(l->save_line),
         fonts_get_system_font(UI_FONT_SMALL), GColorWhite);
}
static __attribute__((noinline)) void mode_picker(GContext *ctx) {
  const Layout *l = &ui_layout;
  const int w = width - 2 * l->inset;
  text(ctx, "HEXSWEEPER", GRect(l->inset, l->picker_title_y, w, l->picker_title_h),
       fonts_get_system_font(UI_FONT_BODY), GColorWhite);
  const char *names[] = {"Classic", "Infinite", "3D", "Save and exit"};
  int first = first_row(mode_choice, l->picker_rows);
  for (int i = first; i < 4 && i < first + l->picker_rows; i++) {
    GRect r = GRect(l->inset, l->picker_top + (i - first) * l->row_h, w, l->row_h);
    if (i == mode_choice) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, r, 3, GCornersAll);
    }
    text(ctx, names[i], inset_text(r, l->row_dy), fonts_get_system_font(UI_FONT_BODY),
         i == mode_choice ? GColorBlack : GColorWhite);
  }
  if (l->picker_footer.h)
    text(ctx, "Boards stay saved", ui_rect(l->picker_footer),
         fonts_get_system_font(UI_FONT_SMALL), GColorWhite);
}
// Keep this frame small: every screen's drawing and firmware text layout
// runs beneath it, and the 64 KB watches have the least stack to spare.
static void draw(Layer *layer, GContext *ctx) {
  uint32_t start = now_ms();
  last_frame = start;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornersAll);
  graphics_context_set_antialiased(ctx, false);
  if (save_failed) {
    text(ctx, "Save interrupted. Select to retry.", ui_rect(ui_layout.save_box),
         fonts_get_system_font(UI_FONT_BODY), GColorWhite);
  } else if (showing_about) {
    about_draw(ctx, width);
  } else if (picker) {
    mode_picker(ctx);
  } else if (app_mode != 2)
    flat_draw(ctx);
  else if (s.mode == HS_PLAY || s.mode == HS_EXPLORE)
    board(ctx);
  else
    menu(ctx);
  uint32_t elapsed = now_ms() - start;
  if (s.gesture.drag && elapsed < 1000)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "HS frame_ms=%lu", (unsigned long)elapsed);
}
static void set_about(bool active) {
  cancel_tap();
  cancel_frame();
  picker_touched = false;
  flat_cancel();
  controller_cancel(&s);
  showing_about = active;
  if (active)
    about_open();
  else
    about_close();
}
static void button(int b) {
  if (showing_about) {
    if (b == HS_SELECT || b == HS_BACK)
      set_about(false);
    trace();
    redraw();
    return;
  }
  if (save_failed) {
    if (b == HS_SELECT)
      record();
    trace();
    redraw();
    return;
  }
  if (picker) {
    if (b == HS_UP)
      mode_choice = (mode_choice + 3) % 4;
    else if (b == HS_DOWN)
      mode_choice = (mode_choice + 1) % 4;
    else if (b == HS_BACK || (b == HS_SELECT && mode_choice == 3)) {
      record();
      if (!save_failed) {
        window_stack_pop_all(false);
        return;
      }
    } else if (b == HS_SELECT) {
      app_mode = mode_choice;
      chose_mode = true;
      picker = false;
      if (app_mode != 2)
        flat_init(app_mode, width, height);
      else if (s.mode == HS_PAUSE)
        s.mode = HS_PLAY;
      record();
    }
    trace();
    redraw();
    if (!picker && app_mode != 2 && flat_busy())
      schedule();
    return;
  }
  if (app_mode != 2) {
    int result = flat_button(b);
    if (result == 3) {
      set_about(true);
      trace();
      redraw();
      return;
    }
    cancel_frame();
    record();
    if (!save_failed && result == 2) {
      window_stack_pop_all(false);
      return;
    }
    if (!save_failed && result == 1) {
      picker = true;
      mode_choice = app_mode;
      flat_cancel();
    }
    trace();
    redraw();
    if (!picker && flat_busy())
      schedule();
    return;
  }
  if (s.mode == HS_PAUSE && s.menu == 5 && b == HS_SELECT) {
    set_about(true);
    trace();
    redraw();
    return;
  }
  if (s.mode == HS_PAUSE && s.menu == 4 && b == HS_SELECT) {
    record();
    if (!save_failed) {
      picker = true;
      mode_choice = 2;
      controller_cancel(&s);
    }
    trace();
    redraw();
    return;
  }
  if (!touch_ready && s.mode == HS_PAUSE && s.menu == 1 && b == HS_SELECT) {
    s.mode = HS_HELP;
    s.help = 3;
    redraw();
    trace();
    return;
  }

  int was = s.mode;
  controller_button(&s, b);
#ifndef PBL_TOUCH
  // Step over the hidden touch row; leaving the confirm returns to New game.
  if (s.mode == HS_PAUSE && was == HS_CONFIRM)
    s.menu = 3;
  else if (s.mode == HS_PAUSE && s.menu == 1)
    s.menu = b == HS_UP ? 0 : 2;
#else
  (void)was;
#endif
  if (!s.gesture.active)
    cancel_tap();
  cancel_frame();
  record();
  trace();
  redraw();
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
#ifdef PBL_TOUCH
static void touch(const TouchEvent *e, void *ctx) {
  const Layout *l = &ui_layout;
  if (!focused)
    return;
  if (e->non_navigational) {
    cancel_tap();
    picker_touched = false;
    flat_cancel();
    controller_cancel(&s);
    return;
  }
  // A phone clock correction can move RTC backwards during a tap. All modes
  // use one event-loop deadline, so a valid tap cannot underflow its age.
  if (e->type == TouchEvent_Touchdown) {
    cancel_tap();
    gesture_clock = 0;
    tap_timer = app_timer_register(350, expire_tap, NULL);
    if (!tap_timer) {
      picker_touched = false;
      flat_cancel();
      controller_cancel(&s);
      return; // No timer: reject this contact, keep buttons available.
    }
  } else if (e->type == TouchEvent_Liftoff) {
    cancel_tap();
  }
  if (showing_about) {
    if (e->type == TouchEvent_Touchdown) {
      picker_touched = true;
      picker_dragged = false;
      picker_down = GPoint(e->x, e->y);
    } else if (picker_touched) {
      if (abs(e->x - picker_down.x) > 8 || abs(e->y - picker_down.y) > 8)
        picker_dragged = true;
      if (e->type == TouchEvent_Liftoff) {
        picker_touched = false;
        if (!picker_dragged && gesture_clock <= 350)
          button(HS_BACK);
      }
    }
    return;
  }
  if (!picker && app_mode == 2 && (s.mode == HS_PAUSE || s.mode == HS_CONFIRM || s.mode == HS_HELP)) {
    if (e->type == TouchEvent_Touchdown) {
      picker_touched = true;
      picker_dragged = false;
      picker_down = GPoint(e->x, e->y);
      picker_time = 0;
    } else if (picker_touched) {
      if (abs(e->x - picker_down.x) > 8 || abs(e->y - picker_down.y) > 8)
        picker_dragged = true;
      if (e->type == TouchEvent_Liftoff) {
        picker_touched = false;
        int dy = e->y - picker_down.y;
        if (picker_dragged) {
          if (s.mode == HS_PAUSE && abs(dy) >= l->row_h)
            button(dy < 0 ? HS_DOWN : HS_UP);
          return;
        }
        if (gesture_clock - picker_time > 350 || e->x < l->inset || e->x >= width - l->inset)
          return;
        if (s.mode == HS_HELP) { button(HS_SELECT); return; }
        int start = s.mode == HS_CONFIRM ? l->confirm_top : l->menu_top;
        int count = s.mode == HS_CONFIRM ? 2 : l->rows;
        if (e->y >= start && e->y < start + count * l->row_h) {
          int first = s.mode == HS_CONFIRM ? 0 : first_row(s.menu, l->rows);
          s.menu = first + (e->y - start) / l->row_h;
          button(HS_SELECT);
        }
      }
    }
    return;
  }
  if (picker) {
    if (e->type == TouchEvent_Touchdown) {
      picker_touched = true;
      picker_dragged = false;
      picker_down = GPoint(e->x, e->y);
      picker_time = 0;
    } else if (picker_touched) {
      if (abs(e->x - picker_down.x) > 8 || abs(e->y - picker_down.y) > 8)
        picker_dragged = true;
      if (e->type == TouchEvent_Liftoff) {
        picker_touched = false;
        if (!picker_dragged && gesture_clock - picker_time <= 350 &&
            e->x >= l->inset && e->x < width - l->inset && e->y >= l->picker_top &&
            e->y < l->picker_top + l->picker_rows * l->row_h) {
          mode_choice = first_row(mode_choice, l->picker_rows) +
                        (e->y - l->picker_top) / l->row_h;
          button(HS_SELECT);
        }
      }
    }
    return;
  }
  if (app_mode != 2) {
    int result = flat_touch(e, gesture_clock);
    if (e->type == TouchEvent_Liftoff) {
      if (result == 3) {
        set_about(true);
        trace();
        redraw();
        return;
      }
      cancel_frame();
      record();
      if (!save_failed && result == 2) {
        window_stack_pop_all(false);
        return;
      }
      if (!save_failed && result == 1) {
        picker = true;
        mode_choice = app_mode;
      }
      trace();
      redraw();
      if (!picker && flat_busy())
        schedule();
    } else
      schedule();
    return;
  }
  int type = e->type == TouchEvent_Touchdown ? HS_TOUCH_DOWN
             : e->type == TouchEvent_Liftoff ? HS_TOUCH_UP
                                             : HS_TOUCH_MOVE;
  if (type == HS_TOUCH_DOWN && (e->y < l->bar_top || e->y >= height - l->bar_bottom)) {
    controller_cancel(&s);
    return;
  }
  controller_touch(&s, type, e->x, e->y, gesture_clock, e->non_navigational);
  if (type == HS_TOUCH_UP || e->non_navigational) {
    cancel_tap();
    cancel_frame();
    record();
    trace();
    redraw();
  } else if (s.gesture.drag)
    schedule();
}
#endif
static void focus(bool active) {
  focused = active;
  flat_cancel();
  picker_touched = false;
  controller_cancel(&s);
  cancel_tap();
  cancel_frame();
  record();
  if (active) {
    redraw();
    if (!picker && app_mode != 2 && flat_busy())
      schedule();
  }
}
static void load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect bounds = layer_get_bounds(root);
  width = bounds.size.w;
  height = bounds.size.h;
  layout_init(&ui_layout, width, height, PBL_IF_ROUND_ELSE(true, false));
  Viewport view = {ui_layout.sphere_cx, ui_layout.sphere_cy, ui_layout.sphere_r};
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
  chose_mode = restored || persist_exists(2);
  if (persist_exists(2)) {
    int mode = persist_read_int(2);
    if (mode >= 0 && mode <= 2)
      app_mode = mode;
    else
      chose_mode = false;
  }
  // Always offer a mode; the last mode remains highlighted and saved.
  picker = true;
  mode_choice = app_mode;
  if (app_mode != 2)
    flat_init(app_mode, width, height);
  canvas = layer_create(bounds);
  layer_set_update_proc(canvas, draw);
  layer_add_child(root, canvas);
  trace();
  if (!picker && app_mode != 2 && flat_busy())
    schedule();
}
static void unload(Window *w) {
  about_close();
  layer_destroy(canvas);
  canvas = NULL;
}
int main(void) {
  WatchInfoVersion firmware = watch_info_get_firmware_version();
#ifdef PBL_TOUCH
  // The firmware gate still precedes every touch call on touch watches.
  touch_ready =
      firmware_allows_touch(firmware.major, firmware.minor, firmware.patch);
#else
  touch_ready = false;
#endif
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
#ifdef PBL_TOUCH
  if (touch_ready)
    touch_service_subscribe(touch, NULL);
#endif
  app_focus_service_subscribe(focus);
  app_event_loop();
  controller_cancel(&s);
  cancel_tap();
  record();
  cancel_frame();
#ifdef PBL_TOUCH
  if (touch_ready)
    touch_service_unsubscribe();
#endif
  app_focus_service_unsubscribe();
  window_destroy(window);
}
