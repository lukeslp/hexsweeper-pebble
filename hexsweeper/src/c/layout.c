// Luke Steuber. One geometry table per display shape, derived from its bounds.
// Time 2 and Round 2 keep the 0.7 values exactly; test/layout.c checks them.
#include "layout.h"
#include <string.h>

Layout ui_layout;

static Box box(int x, int y, int w, int h) {
  return (Box){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
}

int layout_chord(const Layout *l, int y) {
  if (!l->round)
    return l->w / 2;
  int r = l->w / 2, d = y - l->h / 2;
  int v = r * r - d * d, s = 0;
  if (v <= 0)
    return 0;
  while ((s + 1) * (s + 1) <= v)
    s++;
  return s;
}

// Time 2 (200 x 228) and Round 2 (260 x 260): unchanged 0.7 layout.
static void large(Layout *l, int w, int h, bool round) {
  l->hud = HUD_FULL;
  l->inset = round ? 30 : 8;
  l->row_h = 38;
  l->rows = 4;
  l->title_y = 8;
  l->title_h = 36;
  l->title_font = 28;
  l->menu_top = 48;
  l->compact_inset = round ? 48 : 8;
  l->compact_bottom_y = h - (round ? 36 : 28);
  l->menu_footer_y = l->compact_bottom_y;
  l->picker_title_y = 12;
  l->picker_title_h = 38;
  l->picker_top = 54;
  l->picker_rows = 4;
  l->picker_footer = box(30, h - (round ? 45 : 26), w - 60, 24);
  l->confirm_title_y = 8;
  l->confirm_body_y = 68;
  l->confirm_body_h = 62;
  l->confirm_body_font = 24;
  l->confirm_top = 132;
  l->flat_confirm_title_y = 25;
  l->flat_confirm_title_h = 40;
  l->flat_confirm_body_y = 82;
  l->flat_confirm_body_h = 44;
  l->help_body_y = 52;
  l->help_body_h = h - 88;
  l->help_font = 24;
  l->help_hint_y = h - (round ? 45 : 29);
  l->flat_help_title_y = 20;
  l->flat_help_title_h = 32;
  l->flat_help_body_y = 58;
  l->flat_help_body_h = 142;
  l->flat_help_footer_y = l->compact_bottom_y;
  l->classic_top_y = round ? 6 : 2;
  l->field_line1_y = 2;
  l->field_line2_y = 24;
  l->hex_x = round ? 32 : 26;
  l->hex_y = round ? 27 : 22;
  l->hex_half_y = round ? 18 : 15;
  l->board_cy = l->field_cy = h / 2 - 2;
  l->field_min_y = 48 + l->hex_half_y;
  l->field_max_y = h - 33 - l->hex_half_y;
  l->field_radius = round ? w / 2 - l->hex_half_y - 2 : 0;
  l->sphere_cx = w / 2;
  l->sphere_cy = h / 2;
  l->sphere_r = round ? 114 : 94;
  l->bar_top = 28;
  l->bar_bottom = round ? 45 : 33;
  l->sphere_top = round ? box(48, 6, w - 96, 25) : box(8, 0, w - 16, 25);
  l->sphere_bottom = round ? box(47, h - 48, w - 94, 32) : box(8, h - 36, w - 16, 32);
  l->about_title = box(24, 10, w - 48, 37);
  l->about_title_font = 28;
  l->portrait = 96;
  l->portrait_y = 47;
  l->about_name = box(24, 144, w - 48, 32);
  l->about_version = box(24, 176, w - 48, 25);
  l->about_hint = box(24, 203, w - 48, 25);
  l->save_box = box(25, 70, w - 50, 90);
  l->save_line = box(l->inset, h - 27, w - 2 * l->inset, 20);
  l->fail_title_y = 25;
  l->fail_title_h = 50;
  l->fail_body_y = 95;
  l->fail_body_h = 75;
}

// Shared by 144 x 168 and 180 x 180: Gothic 24 titles and rows, Gothic 18
// help and readouts, and a 20-pixel hex pitch that keeps all 37 cells on screen.
static void small(Layout *l, int w, int h, bool round) {
  l->title_font = 24;
  l->about_title_font = 24;
  l->confirm_body_font = round ? 18 : 24;
  l->help_font = 18;
  l->hex_x = 20;
  l->hex_y = 17;
  l->hex_half_y = 11;
  l->portrait = 72;
  l->picker_footer = box(0, 0, 0, 0);
  if (!round) {
    // Readouts own a Gothic 18 line at each edge; the board fills the rest.
    l->hud = HUD_RECT_SMALL;
    l->inset = 6;
    l->row_h = 32;
    l->row_dy = -1;
    l->title_y = -2;
    l->title_h = 32;
    l->menu_top = l->title_y + l->title_h + 2;
    l->rows = (h - l->menu_top) / l->row_h;
    l->compact_inset = 4;
    l->compact_bottom_y = h - 21;
    l->menu_footer_y = -1;
    l->classic_top_y = 0;
    l->field_line1_y = 0;
    l->field_line2_y = -1;
    l->board_cy = l->field_cy = h / 2;
    l->field_min_y = 22 + l->hex_half_y;
    l->field_max_y = l->compact_bottom_y + 4 - l->hex_half_y;
    l->bar_top = 22;
    l->bar_bottom = 31;
    l->sphere_r = w / 2 - 6;
    l->sphere_cy = (l->bar_top + h - l->bar_bottom) / 2;
    l->sphere_top = box(4, 0, w - 8, l->bar_top);
    l->sphere_bottom = box(4, h - l->bar_bottom - 2, w - 8, 33);
    l->confirm_body_y = l->menu_top - 2;
    l->confirm_body_h = 56;
    l->help_body_y = l->menu_top - 2;
    l->help_hint_y = l->compact_bottom_y;
    l->about_title = box(l->inset, -2, w - 2 * l->inset, 32);
    l->portrait_y = 28;
    l->about_name = box(l->inset, 96, w - 2 * l->inset, 30);
    l->about_version = box(l->inset, 122, w - 2 * l->inset, 22);
    l->about_hint = box(l->inset, 143, w - 2 * l->inset, 24);
    l->save_box = box(l->inset, 40, w - 2 * l->inset, 90);
    l->fail_title_y = 10;
    l->fail_body_y = 70;
    l->fail_body_h = 80;
  } else {
    // Text sits inside the display chord at its own height (layout_chord).
    l->hud = HUD_ROUND_SMALL;
    l->inset = 20;
    l->row_h = 34;
    l->title_y = 12;
    l->title_h = 30;
    l->menu_top = l->title_y + l->title_h + 2;
    l->rows = 3;
    l->compact_inset = 20;
    l->compact_bottom_y = 146;
    l->menu_footer_y = l->compact_bottom_y;
    l->classic_top_y = 4;
    l->field_line1_y = 4;
    l->field_line2_y = 21;
    l->board_cy = l->field_cy = h / 2 - 2;
    l->field_min_y = 42 + l->hex_half_y;
    l->field_max_y = l->compact_bottom_y + 4 - l->hex_half_y;
    l->field_radius = w / 2 - l->hex_half_y - 2;
    l->bar_top = 34;
    l->bar_bottom = 42;
    l->sphere_r = 70;
    l->sphere_cy = h / 2;
    l->sphere_top = box(l->inset, 12, w - 2 * l->inset, 22);
    l->sphere_bottom = box(l->inset, h - 46, w - 2 * l->inset, 32);
    l->confirm_body_y = l->menu_top;
    l->confirm_body_h = 40;
    l->help_body_y = l->menu_top - 6;
    l->help_hint_y = 138;
    l->about_title = box(l->inset, 6, w - 2 * l->inset, 30);
    l->portrait_y = 34;
    l->about_name = box(l->inset, 102, w - 2 * l->inset, 30);
    l->about_version = box(l->inset, 126, w - 2 * l->inset, 22);
    l->about_hint = box(l->inset, 145, w - 2 * l->inset, 24);
    l->save_box = box(l->inset, 50, w - 2 * l->inset, 90);
    l->fail_title_y = 30;
    l->fail_body_y = 86;
    l->fail_body_h = 70;
  }
  l->sphere_cx = w / 2;
  l->picker_title_y = l->title_y;
  l->picker_title_h = l->title_h;
  l->picker_top = l->menu_top;
  l->picker_rows = l->rows;
  l->confirm_title_y = l->title_y;
  l->confirm_top = l->round ? 82 : h - 2 * l->row_h - 8;
  l->flat_confirm_title_y = l->title_y;
  l->flat_confirm_title_h = l->title_h;
  l->flat_confirm_body_y = l->confirm_body_y;
  l->flat_confirm_body_h = l->confirm_body_h;
  l->help_body_h = l->help_hint_y - l->help_body_y;
  l->flat_help_title_y = l->title_y;
  l->flat_help_title_h = l->title_h;
  l->flat_help_body_y = l->help_body_y;
  l->flat_help_body_h = l->help_body_h;
  l->flat_help_footer_y = l->help_hint_y;
  l->save_line = box(l->inset, l->compact_bottom_y, w - 2 * l->inset, 20);
  l->fail_title_h = 56;
}

void layout_init(Layout *l, int w, int h, bool round) {
  memset(l, 0, sizeof *l);
  l->w = (int16_t)w;
  l->h = (int16_t)h;
  l->round = round;
  if (w >= 200)
    large(l, w, h, round);
  else
    small(l, w, h, round);
}
