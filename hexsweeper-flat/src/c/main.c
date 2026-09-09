// Flat Hexsweeper. Luke Steuber.
#include "game.h"
#include "geometry.h"
#include <pebble.h>
static Game g;
static Window *win;
static Layer *layer;
static int w, h, selected = 18, screen = 2, menu, sx, sy;
static bool touching, moved, focused = true, touch_on, save_failed;
static AppTimer *timer;
typedef struct {
  uint32_t magic;
  Game game;
  uint8_t selected;
  uint32_t checksum;
} Save;
static uint32_t hash(const void *p, size_t n) {
  const uint8_t *b = p;
  uint32_t v = 2166136261u;
  while (n--)
    v = (v ^ *b++) * 16777619u;
  return v;
}
static void save(void) {
  Save s = {0};
  s.magic = 0x48463231;
  s.game = g;
  s.selected = selected;
  s.checksum = hash(&s, offsetof(Save, checksum));
  save_failed = persist_write_data(1, &s, sizeof s) != sizeof s;
}
static bool restore(void) {
  Save s = {0};
  if (persist_read_data(1, &s, sizeof s) != sizeof s || s.magic != 0x48463231 ||
      s.selected >= 37 || s.checksum != hash(&s, offsetof(Save, checksum)) ||
      !game_valid(&s.game))
    return false;
  g = s.game;
  selected = s.selected;
  return true;
}
static void text(GContext *c, const char *s, int y, int hh, bool big) {
  graphics_context_set_text_color(c, GColorWhite);
  graphics_draw_text(c, s,
                     fonts_get_system_font(big ? FONT_KEY_GOTHIC_24_BOLD
                                               : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(22, y, w - 44, hh), GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);
}
static GPoint center(int i) {
  return GPoint(w / 2 + 22 * XY[i][0] + 11 * XY[i][1],
                h / 2 - 6 + 19 * XY[i][1]);
}
static void hex(GContext *c, GPoint p, int shrink, bool fill) {
  int a = 11 - shrink, b = 12 - shrink;
  GPoint v[] = {GPoint(p.x, p.y - b),         GPoint(p.x + a, p.y - b / 2),
                GPoint(p.x + a, p.y + b / 2), GPoint(p.x, p.y + b),
                GPoint(p.x - a, p.y + b / 2), GPoint(p.x - a, p.y - b / 2)};
  GPath path = {.num_points = 6, .points = v};
  if (fill)
    gpath_draw_filled(c, &path);
  else
    gpath_draw_outline(c, &path);
}
static void row(GContext *c, const char *s, int y, bool on) {
  graphics_context_set_fill_color(c, on ? GColorDarkGray : GColorBlack);
  graphics_fill_rect(c, GRect(26, y, w - 52, 32), 4, GCornersAll);
  text(c, s, y + 2, 28, false);
}
static void draw(Layer *l, GContext *c) {
  graphics_context_set_fill_color(c, GColorBlack);
  graphics_fill_rect(c, GRect(0, 0, w, h), 0, GCornerNone);
  char b[64];
  if (screen == 2) {
    text(c, "HEXSWEEPER FLAT", 18, 36, true);
    text(c,
         "Tap aims; Select digs.\nHold Select to flag.\nUp/Down move focus.\n"
         "Select a clue: open nearby\nif flags match. Back: menu.",
         62, 116, false);
    text(c, "Select to play", h - 36, 26, false);
    return;
  }
  if (screen == 1) {
    text(c, "HEXSWEEPER", 20, 32, true);
    const char *items[] = {"Resume", "How to play", "New board",
                           "Save and exit"};
    for (int i = 0; i < 4; i++)
      row(c, items[i], 58 + i * 34, menu == i);
    return;
  }
  if (screen == 3) {
    text(c, "Replace board?", 35, 40, true);
    row(c, "Keep playing", 112, menu == 0);
    row(c, "New board", 148, menu == 1);
    return;
  }
  snprintf(b, sizeof b, "%d/31 safe   %d flags", game_open_count(&g),
           game_flag_count(&g));
  text(c, b, 18, 24, false);
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < 37; i++) {
      if (pass && i != selected)
        continue;
      GPoint p = center(i);
      int state = g.cells[i];
      bool flagged = state & HS_FLAG;
      bool open = state & HS_OPEN,
           mine = (state & HS_MINE) && g.status == HS_LOST;
      if (pass) {
        graphics_context_set_stroke_width(c, 2);
        graphics_context_set_stroke_color(c, GColorWhite);
        hex(c, p, 0, false);
        graphics_context_set_stroke_color(c, GColorCyan);
        hex(c, p, 2, false);
        graphics_context_set_stroke_width(c, 1);
        continue;
      }
      graphics_context_set_fill_color(c, mine      ? GColorMelon
                                         : flagged ? GColorChromeYellow
                                         : open    ? GColorBlack
                                                   : GColorDarkGray);
      hex(c, p, 1, true);
      graphics_context_set_stroke_color(c, GColorLightGray);
      hex(c, p, 1, false);
      if (mine) {
        graphics_context_set_fill_color(c, GColorBlack);
        graphics_fill_circle(c, p, 3);
        graphics_context_set_stroke_color(c, GColorBlack);
        graphics_draw_line(c, GPoint(p.x - 6, p.y), GPoint(p.x + 6, p.y));
        graphics_draw_line(c, GPoint(p.x, p.y - 6), GPoint(p.x, p.y + 6));
      } else if (flagged) {
        // Solid amber tile, dark pennant and thick pole survive low saturation.
        graphics_context_set_stroke_color(c, GColorBlack);
        graphics_context_set_stroke_width(c, 2);
        graphics_draw_line(c, GPoint(p.x - 3, p.y - 6),
                           GPoint(p.x - 3, p.y + 6));
        graphics_draw_line(c, GPoint(p.x - 6, p.y + 6),
                           GPoint(p.x + 3, p.y + 6));
        GPoint v[] = {GPoint(p.x - 2, p.y - 6), GPoint(p.x + 6, p.y - 3),
                      GPoint(p.x - 2, p.y + 1)};
        GPath a = {.num_points = 3, .points = v};
        graphics_context_set_fill_color(c, GColorBlack);
        gpath_draw_filled(c, &a);
        graphics_context_set_stroke_width(c, 1);
      } else if (open) {
        int n = game_clue(&g, i);
        if (n) {
          char s[2] = {'0' + n, 0};
          graphics_context_set_text_color(c, GColorWhite);
          graphics_draw_text(
              c, s, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
              GRect(p.x - 9, p.y - 12, 18, 23),
              GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        }
      } else {
        graphics_context_set_stroke_color(c, GColorLightGray);
        for (int x = -4; x <= 4; x += 4)
          for (int y = -4; y <= 4; y += 4)
            graphics_draw_pixel(c, GPoint(p.x + x, p.y + y));
      }
    }
  const char *status = g.status == HS_WON            ? "BOARD CLEARED"
                       : g.status == HS_LOST         ? "MINE HIT"
                       : g.cells[selected] & HS_FLAG ? "FLAGGED"
                       : g.cells[selected] & HS_OPEN ? "OPEN"
                                                     : "COVERED";
  text(c, status, h - (h > 240 ? 61 : 42), 23, false);
  text(c, save_failed ? "Save failed" : "Select / hold",
       h - (h > 240 ? 40 : 23), 20, false);
}
static void refresh(void) {
  APP_LOG(APP_LOG_LEVEL_INFO,
          "HF screen=%d selected=%d status=%d opened=%d flags=%d seed=%lu",
          screen, selected, g.status, game_open_count(&g), game_flag_count(&g),
          (unsigned long)g.seed);
  layer_mark_dirty(layer);
}
static void action(int b) {
  if (screen == 2) {
    screen = 0;
  } else if (screen == 3) {
    if (b == 3)
      screen = 1;
    else if (b == 0 || b == 2)
      menu = 1 - menu;
    else {
      if (menu) {
        game_new(&g, (uint32_t)time(NULL));
        selected = 18;
        save();
      }
      screen = 0;
    }
  } else if (screen == 1) {
    if (b == 3)
      screen = 0;
    else if (b == 0)
      menu = (menu + 3) % 4;
    else if (b == 2)
      menu = (menu + 1) % 4;
    else if (menu == 0)
      screen = 0;
    else if (menu == 1)
      screen = 2;
    else if (menu == 2) {
      screen = 3;
      menu = 0;
    } else {
      save();
      if (!save_failed) {
        window_stack_pop_all(false);
        return;
      }
    }
  } else if (b == 3) {
    screen = 1;
    menu = 0;
  } else {
    if (b == 0)
      selected = (selected + 36) % 37;
    else if (b == 2)
      selected = (selected + 1) % 37;
    else
      game_reveal(&g, selected);
    save();
  }
  refresh();
}
static void up(ClickRecognizerRef r, void *x) { action(0); }
static void down(ClickRecognizerRef r, void *x) { action(2); }
static void sel(ClickRecognizerRef r, void *x) { action(1); }
static void back(ClickRecognizerRef r, void *x) { action(3); }
static void flag(ClickRecognizerRef r, void *x) {
  if (!screen) {
    game_flag(&g, selected);
    save();
    refresh();
  }
}
static void clicks(void *x) {
  window_single_click_subscribe(BUTTON_ID_UP, up);
  window_single_click_subscribe(BUTTON_ID_DOWN, down);
  window_single_click_subscribe(BUTTON_ID_SELECT, sel);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, flag, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back);
}
static void cancel(void) {
  touching = false;
  if (timer) {
    app_timer_cancel(timer);
    timer = NULL;
  }
}
static void expire(void *x) {
  timer = NULL;
  touching = false;
}
static void touch(const TouchEvent *e, void *x) {
  if (!focused)
    return;
  if (e->type == TouchEvent_Touchdown) {
    cancel();
    sx = e->x;
    sy = e->y;
    moved = false;
    touching = true;
    timer = app_timer_register(350, expire, NULL);
  } else if (e->type == TouchEvent_PositionUpdate) {
    if (abs(e->x - sx) > 8 || abs(e->y - sy) > 8)
      moved = true;
  } else {
    bool ok = touching && !moved;
    cancel();
    if (!ok)
      return;
    if (!screen) {
      int best = -1, distance = 100000;
      for (int i = 0; i < 37; i++) {
        GPoint p = center(i);
        int dx = sx - p.x, dy = sy - p.y, d = dx * dx + dy * dy;
        if (d < distance) {
          distance = d;
          best = i;
        }
      }
      if (distance <= 121) {
        selected = best;
        save();
        refresh();
      }
    } else if (screen == 2)
      action(1);
    else if (screen == 1 && sy >= 58 && sy < 194) {
      menu = (sy - 58) / 34;
      action(1);
    } else if (screen == 3 && sy >= 112 && sy < 180) {
      menu = sy >= 148;
      action(1);
    }
  }
}
static void focus(bool v) {
  focused = v;
  cancel();
  if (!v)
    save();
}
static void load(Window *x) {
  GRect b = layer_get_bounds(window_get_root_layer(x));
  w = b.size.w;
  h = b.size.h;
  layer = layer_create(b);
  layer_set_update_proc(layer, draw);
  layer_add_child(window_get_root_layer(x), layer);
  refresh();
}
static void unload(Window *x) { layer_destroy(layer); }
int main(void) {
  if (restore())
    screen = 0;
  else
    game_new(&g, (uint32_t)time(NULL));
  win = window_create();
  window_set_background_color(win, GColorBlack);
  window_set_window_handlers(win,
                             (WindowHandlers){.load = load, .unload = unload});
  window_set_click_config_provider(win, clicks);
  window_stack_push(win, false);
  WatchInfoVersion v = watch_info_get_firmware_version();
  touch_on = v.major > 4 || (v.major == 4 &&
                             (v.minor > 33 || (v.minor == 33 && v.patch >= 2)));
  if (touch_on)
    touch_service_subscribe(touch, NULL);
  app_focus_service_subscribe(focus);
  app_event_loop();
  cancel();
  save();
  if (touch_on)
    touch_service_unsubscribe();
  app_focus_service_unsubscribe();
  window_destroy(win);
}
