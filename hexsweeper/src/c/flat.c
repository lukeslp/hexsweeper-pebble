// Luke Steuber. Flat boards share the application window and mode picker.
#include "flat.h"
#include "classic.h"
#include "classic_geometry.h"
#include "controller.h"
#include "infinite.h"
#include "world_save.h"
#include "ui_style.h"
#include <limits.h>
static ClassicGame finite;
static Infinite world;
static int kind, w, h, ui, menu, selected = 18, direction, help;
static bool flag_tool, failed, touched, dragged;
static GPoint down_point;
static InfCoord down_focus;
static const InfIO io = {persist_read_data, persist_write_data};
typedef struct {
  ClassicGame game;
  uint32_t selected, checksum;
} ClassicSave;
static uint32_t hash(const void *v, size_t n) {
  const uint8_t *p = v;
  uint32_t s = 2166136261u;
  while (n--)
    s = (s ^ *p++) * 16777619u;
  return s;
}
bool flat_save(void) {
  if (kind)
    failed = !inf_store(&io, &world);
  else {
    ClassicSave s = {0};
    s.game = finite;
    s.selected = selected;
    s.checksum = hash(&s, offsetof(ClassicSave, checksum));
    failed = persist_write_data(20, &s, sizeof s) != sizeof s;
  }
  return !failed;
}
void flat_init(int mode, int width, int height) {
  kind = mode;
  w = width;
  h = height;
  ui = 0;
  menu = 0;
  flag_tool = false;
  failed = false;
  touched = false;
  if (kind) {
    if (!inf_load(&io, &world)) {
      inf_new(&world, (uint32_t)time(NULL));
      ui = 3;
      help = 0;
    }
  } else {
    ClassicSave s;
    if (persist_read_data(20, &s, sizeof s) == sizeof s && s.selected < 37 &&
        hash(&s, offsetof(ClassicSave, checksum)) == s.checksum &&
        classic_valid(&s.game)) {
      finite = s.game;
      selected = s.selected;
    } else {
      classic_new(&finite, (uint32_t)time(NULL));
      selected = 18;
      ui = 3;
      help = 0;
    }
  }
}
void flat_cancel(void) { touched = false; }
static GPoint center(int q, int r) {
  return GPoint(w / 2 + UI_HEX_X * q + UI_HEX_HALF_X * r,
                h / 2 - 2 + UI_HEX_Y * r);
}
static void text(GContext *c, const char *s, int y, int hh, bool big) {
  graphics_context_set_text_color(c, GColorWhite);
  graphics_draw_text(c, s,
                     fonts_get_system_font(big ? UI_FONT_TITLE : UI_FONT_BODY),
                     GRect(UI_CONTENT_INSET, y, w - UI_CONTENT_INSET * 2, hh), GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);
}
static void row(GContext *c, const char *s, int y, bool on) {
  graphics_context_set_fill_color(c, on ? GColorWhite : GColorBlack);
  GRect r = GRect(UI_CONTENT_INSET, y, w - UI_CONTENT_INSET * 2, UI_ROW_HEIGHT);
  graphics_fill_rect(c, r, 3, GCornersAll);
  graphics_context_set_text_color(c, on ? GColorBlack : GColorWhite);
  graphics_draw_text(c, s, fonts_get_system_font(UI_FONT_BODY), r,
                    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
static void compact(GContext *c, const char *s, int y) {
  if (y > h - 55) y = h - PBL_IF_ROUND_ELSE(36, 28);
  graphics_context_set_text_color(c, GColorWhite);
  int inset = PBL_IF_ROUND_ELSE(48, 8);
  graphics_draw_text(c, s, fonts_get_system_font(UI_FONT_SMALL),
      GRect(inset, y, w - inset * 2, 24), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter, NULL);
}
static int menu_first(void) { return menu > 3 ? menu - 3 : 0; }
static void tile(GContext *c, GPoint p, int state, int clue, bool mine,
                 bool focus) {
  const int a = UI_HEX_HALF_X, b = UI_HEX_HALF_Y;
  GPoint pts[] = {{p.x - a, p.y - b / 2}, {p.x, p.y - b}, {p.x + a, p.y - b / 2},
                  {p.x + a, p.y + b / 2}, {p.x, p.y + b}, {p.x - a, p.y + b / 2}};
  GPath path = {.num_points = 6, .points = pts};
  bool open = state & INF_OPEN, flag = state & INF_FLAG;
  graphics_context_set_fill_color(c, mine   ? GColorMelon
                                     : flag ? GColorChromeYellow
                                     : open ? GColorWhite
                                            : GColorDarkGray);
  gpath_draw_filled(c, &path);
  graphics_context_set_stroke_color(c, focus ? GColorWhite : GColorLightGray);
  graphics_context_set_stroke_width(c, focus ? 5 : 1);
  gpath_draw_outline(c, &path);
  if (focus) {
    graphics_context_set_stroke_color(c, GColorBlack);
    graphics_context_set_stroke_width(c, 3);
    gpath_draw_outline(c, &path);
    graphics_context_set_stroke_color(c, GColorCyan);
    graphics_context_set_stroke_width(c, 1);
    gpath_draw_outline(c, &path);
  }
  graphics_context_set_stroke_width(c, 1);
  if (flag) {
    graphics_context_set_stroke_color(c, GColorBlack);
    graphics_context_set_stroke_width(c, 2);
    graphics_draw_line(c, GPoint(p.x - 4, p.y - 7), GPoint(p.x - 4, p.y + 7));
    graphics_draw_line(c, GPoint(p.x - 7, p.y + 7), GPoint(p.x + 3, p.y + 7));
    GPoint fp[] = {{p.x - 3, p.y - 7}, {p.x + 6, p.y - 3}, {p.x - 3, p.y + 1}};
    GPath f = {.num_points = 3, .points = fp};
    graphics_context_set_fill_color(c, GColorBlack);
    gpath_draw_filled(c, &f);
  } else if (mine) {
    graphics_context_set_fill_color(c, GColorBlack);
    graphics_fill_circle(c, p, 4);
  } else if (open && clue) {
    char n[2] = {(char)('0' + clue), 0};
    graphics_context_set_text_color(c, GColorBlack);
    graphics_draw_text(c, n, fonts_get_system_font(UI_FONT_BODY),
                       GRect(p.x - 13, p.y - 16, 26, 30),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  } else if (!open) {
    graphics_context_set_stroke_color(c, GColorLightGray);
    graphics_draw_pixel(c, GPoint(p.x - 3, p.y - 3));
    graphics_draw_pixel(c, GPoint(p.x + 3, p.y + 3));
  }
  graphics_context_set_stroke_width(c, 1);
}
static bool visible(GPoint p) {
  if (p.x < UI_HEX_HALF_X + 2 || p.x > w - UI_HEX_HALF_X - 2 ||
      p.y < 48 + UI_HEX_HALF_Y || p.y > h - 33 - UI_HEX_HALF_Y)
    return false;
#ifdef PBL_ROUND
  int x = p.x - w / 2, y = p.y - h / 2;
  return x * x + y * y < (w / 2 - UI_HEX_HALF_Y - 2) * (w / 2 - UI_HEX_HALF_Y - 2);
#else
  return true;
#endif
}
static bool relative(int q, int r, InfCoord *p) {
  int64_t x = (int64_t)world.focus.q + q, y = (int64_t)world.focus.r + r;
  if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX)
    return false;
  *p = (InfCoord){x, y};
  return true;
}
void flat_draw(GContext *c) {
  char b[80];
  if (failed) {
    text(c, "SAVE INTERRUPTED", 25, 50, true);
    text(c, "Select to retry.\nPlay pauses until saved.", 95, 75, false);
    return;
  }
  if (ui == 2) {
    text(c, kind ? "INFINITE" : "CLASSIC", 8, 36, true);
    const char *items[] = {"Resume", "Controls", "New board", "Game modes",
                           "Save and exit"};
    for (int i = menu_first(); i < 5 && i < menu_first() + 4; i++)
      row(c, items[i], 48 + (i - menu_first()) * UI_ROW_HEIGHT, menu == i);
    snprintf(b, sizeof b, "%d/5", menu + 1);
    compact(c, b, h - 26);
    return;
  }
  if (ui == 4) {
    text(c, "Replace board?", 25, 40, true);
    text(c, "Only this mode resets.", 82, 44, false);
    row(c, "Keep playing", 132, menu == 0);
    row(c, "New board", 170, menu == 1);
    return;
  }
  if (ui == 3) {
    text(c, kind ? "INFINITE" : "CLASSIC", 20, 32, true);
    const char *body =
        kind ? (help ? "Tiles stay saved.\nStorage full?\nKeep playing in\nsaved areas, or\nstart a new field."
                     : "Drag pans. Tap aims.\nSelect digs; hold flags.\nUp: move. Down: tool.\nSelect clues: chord.\nBack: menu.")
             : "Tap aims. Select digs.\nHold Select: flag.\nUp/Down: move focus.\nSelect clues: chord.\nClear 31 safe cells.";
    text(c, body, 58, 142, false);
    compact(c, kind ? "Up/Down: help" : "Select: play", h - 27);
    return;
  }
  if (kind) {
    snprintf(b, sizeof b, "%lu safe  %lu flags", (unsigned long)world.opened, (unsigned long)world.flagged);
    compact(c, b, 2);
    snprintf(b, sizeof b, "%ld, %ld", (long)world.focus.q, (long)world.focus.r);
    compact(c, b, 24);
    InfCoord highlight = world.focus;
    if (ui == 1)
      inf_neighbor(world.focus, direction, &highlight);
    for (int pass = 0; pass < 2; pass++)
      for (int r = -5; r <= 5; r++)
        for (int q = -7; q <= 7; q++) {
          GPoint p = center(q, r);
          InfCoord cell;
          if (!visible(p) || !relative(q, r, &cell))
            continue;
          bool focus = cell.q == highlight.q && cell.r == highlight.r;
          if (pass && !focus)
            continue;
          int state = inf_state(&world, cell);
          tile(c, p, state, (state & INF_OPEN) ? inf_clue(&world, cell) : 0,
               world.status == INF_LOST && inf_mine(&world, cell),
               pass && focus);
        }
    if (world.status == INF_LOST)
      snprintf(b, sizeof b, "MINE - Back: menu");
    else if (world.full)
      snprintf(b, sizeof b, "Save space full");
    else if (ui == 1)
      snprintf(b, sizeof b, "EXPLORE %d/6", direction + 1);
    else
      snprintf(b, sizeof b, "%s  %lu flags", flag_tool ? "FLAG" : "DIG",
               (unsigned long)world.flagged);
    compact(c, b, h - 29);
  } else {
    snprintf(b, sizeof b, "%d/31 safe  %d flags", classic_open_count(&finite),
             classic_flag_count(&finite));
    compact(c, b, PBL_IF_ROUND_ELSE(6, 2));
    for (int pass = 0; pass < 2; pass++)
      for (int i = 0; i < 37; i++) {
        if (pass && i != selected)
          continue;
        int s = finite.cells[i], state = ((s & FS_OPEN) ? INF_OPEN : 0) |
                                         ((s & FS_FLAG) ? INF_FLAG : 0);
        tile(c, center(FS_XY[i][0], FS_XY[i][1]), state,
             classic_clue(&finite, i),
             finite.status == FS_LOST && (s & FS_MINE), pass && i == selected);
      }
    compact(c,
         finite.status == FS_WON    ? "CLEARED"
         : finite.status == FS_LOST ? "MINE - Back: menu"
                                    : "Dig · hold to flag",
         h - 28);
  }
}
int flat_button(int b) {
  touched = false;
  if (failed) {
    if (b == HS_SELECT)
      flat_save();
    return 0;
  }
  if (ui == 3) {
    if (b == HS_UP || b == HS_DOWN)
      help = !help;
    else if (b == HS_SELECT || b == HS_BACK)
      ui = 0;
    return 0;
  }
  if (ui == 2) {
    if (b == HS_BACK)
      ui = 0;
    else if (b == HS_UP)
      menu = (menu + 4) % 5;
    else if (b == HS_DOWN)
      menu = (menu + 1) % 5;
    else if (b == HS_SELECT) {
      if (menu == 0)
        ui = 0;
      else if (menu == 1) {
        ui = 3;
        help = 0;
      } else if (menu == 2) {
        ui = 4;
        menu = 0;
      } else
        return menu == 3 ? 1 : 2;
    }
    return 0;
  }
  if (ui == 4) {
    if (b == HS_BACK)
      ui = 2;
    else if (b == HS_UP || b == HS_DOWN)
      menu = 1 - menu;
    else if (b == HS_SELECT) {
      if (menu) {
        if (kind)
          inf_new(&world, world.seed ^ (uint32_t)time(NULL) ^ 0xa561);
        else {
          classic_new(&finite, finite.seed ^ (uint32_t)time(NULL) ^ 0xa561);
          selected = 18;
        }
      }
      ui = 0;
    }
    return 0;
  }
  if (kind && ui == 1) {
    if (b == HS_BACK)
      ui = 0;
    else if (b == HS_UP)
      direction = (direction + 5) % 6;
    else if (b == HS_DOWN)
      direction = (direction + 1) % 6;
    else if (b == HS_SELECT) {
      InfCoord p;
      if (inf_neighbor(world.focus, direction, &p))
        world.focus = p;
    }
    return 0;
  }
  if (b == HS_BACK) {
    ui = 2;
    menu = 0;
    return 0;
  }
  if (kind) {
    if (b == HS_UP) {
      ui = 1;
      direction = 0;
    } else if (b == HS_DOWN)
      flag_tool = !flag_tool;
    else if (b == HS_HOLD_SELECT || (b == HS_SELECT && flag_tool))
      inf_flag(&world, world.focus);
    else if (b == HS_SELECT)
      inf_reveal(&world, world.focus);
  } else {
    if (b == HS_UP)
      selected = (selected + 36) % 37;
    else if (b == HS_DOWN)
      selected = (selected + 1) % 37;
    else if (b == HS_HOLD_SELECT)
      classic_flag(&finite, selected);
    else if (b == HS_SELECT)
      classic_reveal(&finite, selected);
  }
  return 0;
}
int flat_touch(const TouchEvent *e, uint32_t tap_age) {
  if (e->non_navigational) {
    touched = false;
    return 0;
  }
  if (e->type == TouchEvent_Touchdown) {
    touched = true;
    dragged = false;
    down_point = GPoint(e->x, e->y);
    down_focus = world.focus;
    return 0;
  }
  if (!touched)
    return 0;
  int dx = e->x - down_point.x, dy = e->y - down_point.y;
  if (abs(dx) > 8 || abs(dy) > 8)
    dragged = true;
  if (kind && ui < 2 && dragged && !failed) {
    int r = -dy / UI_HEX_Y, q = (-dx - UI_HEX_HALF_X * r) / UI_HEX_X;
    int64_t x = (int64_t)down_focus.q + q, y = (int64_t)down_focus.r + r;
    if (x >= INT32_MIN && x <= INT32_MAX && y >= INT32_MIN && y <= INT32_MAX)
      world.focus = (InfCoord){x, y};
  }
  if (e->type != TouchEvent_Liftoff)
    return 0;
  touched = false;
  if (dragged && ui == 2 && abs(dy) >= UI_ROW_HEIGHT)
    return flat_button(dy < 0 ? HS_DOWN : HS_UP);
  if (dragged || tap_age > 350)
    return 0;
  if (failed)
    return flat_button(HS_SELECT);
  if (ui == 3)
    return flat_button(HS_SELECT);
  if (ui == 2 && e->y >= 48 && e->y < 48 + 4 * UI_ROW_HEIGHT) {
    menu = menu_first() + (e->y - 48) / UI_ROW_HEIGHT;
    return flat_button(HS_SELECT);
  }
  if (ui == 4 && e->y >= 132 && e->y < 170 + UI_ROW_HEIGHT) {
    menu = e->y >= 170;
    return flat_button(HS_SELECT);
  }
  if (ui >= 2)
    return 0;
  int best = UI_HEX_HALF_Y * UI_HEX_HALF_Y;
  if (kind) {
    InfCoord target = world.focus;
    for (int r = -5; r <= 5; r++)
      for (int q = -7; q <= 7; q++) {
        GPoint p = center(q, r);
        InfCoord cell;
        int d = (p.x - e->x) * (p.x - e->x) + (p.y - e->y) * (p.y - e->y);
        if (d < best && visible(p) && relative(q, r, &cell)) {
          best = d;
          target = cell;
        }
      }
    world.focus = target;
  } else
    for (int i = 0; i < 37; i++) {
      GPoint p = center(FS_XY[i][0], FS_XY[i][1]);
      int d = (p.x - e->x) * (p.x - e->x) + (p.y - e->y) * (p.y - e->y);
      if (d < best) {
        best = d;
        selected = i;
      }
    }
  return 0;
}
bool flat_busy(void) { return kind && ui < 2 && !failed && world.busy; }
void flat_tick(void) {
  if (flat_busy()) {
    inf_pump(&world, 64);
    if (!world.busy) {
      flat_save();
      flat_trace();
    }
  }
}
void flat_trace(void) {
  if (kind) APP_LOG(APP_LOG_LEVEL_INFO,"HF storage chunks=%d full=%d cell=%d",world.count,world.full,inf_state(&world,world.focus));
  if (kind)
    APP_LOG(APP_LOG_LEVEL_INFO,
            "HF kind=1 ui=%d q=%ld r=%ld status=%d open=%lu flags=%lu busy=%d",
            ui, (long)world.focus.q, (long)world.focus.r, world.status,
            (unsigned long)world.opened, (unsigned long)world.flagged,
            world.busy);
  else
    APP_LOG(APP_LOG_LEVEL_INFO,
            "HF kind=0 ui=%d selected=%d status=%d open=%d flags=%d", ui,
            selected, finite.status, classic_open_count(&finite),
            classic_flag_count(&finite));
}
