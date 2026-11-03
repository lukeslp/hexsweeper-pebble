// Luke Steuber. Screen geometry for every supported Pebble display.
// Plain C without pebble.h so the host suite can check every screen size.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int16_t x, y, w, h;
} Box;

// Readout wording follows the space available, not the platform name.
enum { HUD_FULL, HUD_RECT_SMALL, HUD_ROUND_SMALL };

typedef struct {
  int16_t w, h;
  bool round;
  uint8_t hud;
  // Menus, titles and help. Fonts are Gothic Bold point sizes.
  int16_t inset, row_h, rows, row_dy;
  int16_t title_y, title_h, title_font;
  int16_t menu_top, menu_footer_y;
  int16_t picker_title_y, picker_title_h, picker_top, picker_rows;
  Box picker_footer;
  int16_t confirm_title_y, confirm_body_y, confirm_body_h, confirm_body_font;
  int16_t confirm_top;
  int16_t flat_confirm_title_y, flat_confirm_title_h;
  int16_t flat_confirm_body_y, flat_confirm_body_h;
  int16_t help_body_y, help_body_h, help_font, help_hint_y;
  int16_t flat_help_title_y, flat_help_title_h;
  int16_t flat_help_body_y, flat_help_body_h, flat_help_footer_y;
  // Compact Gothic 18 readouts on the flat boards.
  int16_t compact_inset, compact_bottom_y, classic_top_y;
  int16_t field_line1_y, field_line2_y;
  // Hex boards: pitch, row step, cell circumradius and centers.
  int16_t hex_x, hex_y, hex_half_y;
  int16_t board_cy, field_cy, field_min_y, field_max_y, field_radius;
  // Sphere and its opaque readout bars.
  int16_t sphere_cx, sphere_cy, sphere_r, bar_top, bar_bottom;
  Box sphere_top, sphere_bottom;
  // About and interrupted-save screens.
  Box about_title, about_name, about_version, about_hint;
  int16_t about_title_font, portrait, portrait_y;
  Box save_box, save_line;
  int16_t fail_title_y, fail_title_h, fail_body_y, fail_body_h;
} Layout;

// The running app's layout, set once its window knows the display bounds.
extern Layout ui_layout;
void layout_init(Layout *l, int w, int h, bool round);
// Half-width of a round display's chord through pixel row y.
int layout_chord(const Layout *l, int y);
