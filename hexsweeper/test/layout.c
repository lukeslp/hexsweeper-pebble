// Luke Steuber. Host checks for every display layout.
// Time 2 and Round 2 must keep their 0.7 geometry; the smaller screens must
// keep all 37 Classic cells, the sphere and every menu row on the display.
#include "classic_geometry.h"
#include "layout.h"
#include <assert.h>
#include <stdio.h>

static int failures;
#define CHECK(cond, ...)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      failures++;                                                              \
      printf("FAIL %s:%d ", __FILE__, __LINE__);                               \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  const char *name;
  int w, h;
  bool round;
} Screen;

static const Screen SCREENS[] = {{"basalt/diorite/flint", 144, 168, false},
                                 {"chalk", 180, 180, true},
                                 {"emery", 200, 228, false},
                                 {"gabbro", 260, 260, true}};

// Gothic Bold glyph rows below a text box's top, measured in native captures:
// 18 draws ink from +6 to +20, 24 from +10 to +27, 28 from +10 to +27.
static int ink_top(int font) { return font >= 24 ? 10 : 6; }
static int ink_bottom(int font) { return font >= 24 ? 27 : 20; }

static bool on_display(const Layout *l, int x, int y) {
  if (x < 0 || y < 0 || x >= l->w || y >= l->h)
    return false;
  if (!l->round)
    return true;
  int dx = 2 * x + 1 - l->w, dy = 2 * y + 1 - l->h;
  return dx * dx + dy * dy <= l->w * l->w;
}

// Every cell vertex is on the display and clear of the readout ink.
static void check_hex(const Layout *l, int x, int y, int top, int bottom,
                      const char *what) {
  int a = l->hex_x / 2, b = l->hex_half_y;
  int px[6] = {x - a, x, x + a, x + a, x, x - a};
  int py[6] = {y - b / 2, y - b, y - b / 2, y + b / 2, y + b, y + b / 2};
  for (int i = 0; i < 6; i++) {
    CHECK(on_display(l, px[i], py[i]), "%s vertex (%d,%d) off %dx%d", what,
          px[i], py[i], l->w, l->h);
    CHECK(py[i] > top && py[i] < bottom, "%s vertex y=%d outside %d..%d", what,
          py[i], top, bottom);
  }
}

static void check_rows(const Layout *l, int top, int rows, const char *what) {
  CHECK(top + rows * l->row_h <= l->h, "%s rows end at %d", what,
        top + rows * l->row_h);
  for (int i = 0; i < rows; i++) {
    int y0 = top + i * l->row_h, y1 = y0 + l->row_h - 1;
    // Rounded corners (radius 3) may touch the bezel; text area may not.
    CHECK(on_display(l, l->inset + 2, y0 + 2) && on_display(l, l->w - l->inset - 3, y1 - 2),
          "%s row %d leaves the display", what, i);
  }
}

static void check_screen(const Screen *s) {
  Layout l;
  layout_init(&l, s->w, s->h, s->round);
  printf("%s: rows %d x %d px, hex %d/%d/%d, sphere r=%d\n", s->name, l.rows,
         l.row_h, l.hex_x, l.hex_y, l.hex_half_y, l.sphere_r);
  // Classic: all 37 cells, clear of the top and bottom readouts.
  int top = l.classic_top_y + ink_bottom(18), bottom = l.compact_bottom_y + ink_top(18);
  for (int i = 0; i < 37; i++) {
    int q = FS_XY[i][0], r = FS_XY[i][1];
    check_hex(&l, l.w / 2 + l.hex_x * q + l.hex_x / 2 * r, l.board_cy + l.hex_y * r,
              top, bottom, "classic");
  }
  // Infinite: every drawn cell fits, the focus is drawn, and the field is
  // at least five cells tall and wide.
  int ftop = (l.field_line2_y >= 0 ? l.field_line2_y : l.field_line1_y) + ink_bottom(18);
  int shown = 0, rows_seen = 0;
  for (int r = -5; r <= 5; r++) {
    int in_row = 0;
    for (int q = -7; q <= 7; q++) {
      int x = l.w / 2 + l.hex_x * q + l.hex_x / 2 * r, y = l.field_cy + l.hex_y * r;
      int a = l.hex_x / 2;
      bool visible = x >= a + 2 && x <= l.w - a - 2 && y >= l.field_min_y &&
                     y <= l.field_max_y;
      if (visible && l.round) {
        int dx = x - l.w / 2, dy = y - l.h / 2;
        visible = dx * dx + dy * dy < l.field_radius * l.field_radius;
      }
      if (!visible)
        continue;
      check_hex(&l, x, y, ftop, bottom, "infinite");
      shown++;
      in_row++;
    }
    rows_seen += in_row > 0;
    if (r == 0)
      CHECK(in_row >= 5, "infinite center row has %d cells", in_row);
  }
  CHECK(rows_seen >= 5 && shown >= 25, "infinite shows %d cells in %d rows", shown,
        rows_seen);
  CHECK(l.field_cy >= l.field_min_y && l.field_cy <= l.field_max_y,
        "infinite focus row hidden");
  // Sphere: a circle, uniformly scaled and centered, mostly clear of the bars.
  CHECK(l.sphere_cx - l.sphere_r >= 0 && l.sphere_cx + l.sphere_r <= l.w,
        "sphere wider than the display");
  CHECK(l.sphere_cx == l.w / 2, "sphere off center");
  if (l.round)
    CHECK(l.sphere_r <= l.w / 2 - 10 && l.sphere_cy == l.h / 2,
          "round sphere must sit inside the bezel");
  int band = l.h - l.bar_bottom - l.bar_top;
  CHECK(band * 100 >= 145 * l.sphere_r, "sphere band %d too short for r=%d", band,
        l.sphere_r);
  CHECK(l.sphere_cy > l.bar_top && l.sphere_cy < l.h - l.bar_bottom,
        "sphere center under a readout");
  CHECK(l.sphere_top.y + ink_bottom(18) <= l.bar_top &&
            l.sphere_bottom.y + ink_top(24) >= l.h - l.bar_bottom,
        "sphere readout ink outside its opaque bar");
  // Menus, picker and confirmation rows.
  check_rows(&l, l.menu_top, l.rows, "menu");
  check_rows(&l, l.picker_top, l.picker_rows, "picker");
  check_rows(&l, l.confirm_top, 2, "confirm");
  CHECK(l.title_y + ink_bottom(l.title_font) <= l.menu_top, "title runs into rows");
  CHECK(l.picker_rows >= 3 && l.rows >= 3, "fewer than three rows");
  // Help pages leave the hint line clear.
  // Small screens size the body box to stop at the hint. (Large screens keep
  // their 0.7 boxes, whose five lines end well above the hint.)
  if (l.hud != HUD_FULL)
    CHECK(l.help_body_y + l.help_body_h <= l.help_hint_y + 1, "help body over hint");
  CHECK(on_display(&l, l.w / 2, l.help_hint_y + ink_bottom(18)), "help hint off");
  // About: title, portrait, name, version and hint stack without overlap.
  CHECK(l.about_title.y + ink_bottom(l.about_title_font) <= l.portrait_y + 2,
        "about title over portrait");
  CHECK(l.portrait_y + l.portrait <= l.about_name.y + ink_top(24),
        "portrait over name");
  CHECK(l.about_name.y + ink_bottom(24) <= l.about_version.y + ink_top(18) + 1,
        "name over version");
  CHECK(l.about_version.y + ink_bottom(18) <= l.about_hint.y + ink_top(18),
        "version over hint");
  CHECK(on_display(&l, l.w / 2, l.about_hint.y + ink_bottom(18)), "about hint off");
  // Round text stays inside the chord at its own height for its widest
  // measured string (pixels, from native captures at these fonts).
  if (l.round && l.hud == HUD_ROUND_SMALL) {
    struct {
      int y, font, width;
      const char *what;
    } text[] = {{l.sphere_top.y, 18, 92, "0/35 · 0 flags"},
                {l.sphere_bottom.y, 24, 65, "MOVE 1/6"},
                {l.classic_top_y, 18, 66, "31/31 safe"},
                {l.compact_bottom_y, 18, 80, "MINE · Back"},
                {l.title_y, 24, 106, "HOW TO PLAY"},
                {l.help_hint_y, 18, 100, "1/4  Select: play"},
                {l.about_title.y, 24, 50, "About"},
                {l.about_hint.y, 18, 90, "Select or Back"}};
    for (unsigned i = 0; i < sizeof text / sizeof *text; i++) {
      int y0 = text[i].y + ink_top(text[i].font), y1 = text[i].y + ink_bottom(text[i].font);
      int half = layout_chord(&l, y0) < layout_chord(&l, y1) ? layout_chord(&l, y0)
                                                             : layout_chord(&l, y1);
      CHECK(text[i].width / 2 + 2 <= half, "\"%s\" (%d px) exceeds chord %d at y=%d",
            text[i].what, text[i].width, 2 * half, text[i].y);
    }
  }
}

int main(void) {
  // Time 2 and Round 2 keep the published 0.7 geometry exactly.
  Layout e, g;
  layout_init(&e, 200, 228, false);
  layout_init(&g, 260, 260, true);
  assert(e.hud == HUD_FULL && g.hud == HUD_FULL);
  assert(e.inset == 8 && g.inset == 30 && e.row_h == 38 && g.row_h == 38);
  assert(e.rows == 4 && g.rows == 4 && e.row_dy == 0 && g.row_dy == 0);
  assert(e.title_y == 8 && e.title_h == 36 && e.title_font == 28);
  assert(e.menu_top == 48 && e.picker_top == 54 && e.picker_title_y == 12);
  assert(e.picker_footer.y == 202 && g.picker_footer.y == 215 && e.picker_footer.x == 30);
  assert(e.confirm_top == 132 && e.confirm_body_y == 68 && e.confirm_body_h == 62);
  assert(e.flat_confirm_title_y == 25 && e.flat_confirm_body_y == 82);
  assert(e.help_body_y == 52 && e.help_body_h == 140 && g.help_body_h == 172);
  assert(e.help_hint_y == 199 && g.help_hint_y == 215);
  assert(e.flat_help_title_y == 20 && e.flat_help_body_y == 58 && e.flat_help_body_h == 142);
  assert(e.compact_bottom_y == 200 && g.compact_bottom_y == 224);
  assert(e.compact_inset == 8 && g.compact_inset == 48);
  assert(e.classic_top_y == 2 && g.classic_top_y == 6);
  assert(e.field_line1_y == 2 && e.field_line2_y == 24);
  assert(e.hex_x == 26 && e.hex_y == 22 && e.hex_half_y == 15);
  assert(g.hex_x == 32 && g.hex_y == 27 && g.hex_half_y == 18);
  assert(e.board_cy == 112 && g.board_cy == 128 && e.field_cy == 112);
  assert(e.field_min_y == 63 && e.field_max_y == 180 && e.field_radius == 0);
  assert(g.field_min_y == 66 && g.field_max_y == 209 && g.field_radius == 110);
  assert(e.sphere_cx == 100 && e.sphere_cy == 114 && e.sphere_r == 94);
  assert(g.sphere_cx == 130 && g.sphere_cy == 130 && g.sphere_r == 114);
  assert(e.bar_top == 28 && e.bar_bottom == 33 && g.bar_bottom == 45);
  assert(e.sphere_top.x == 8 && e.sphere_top.y == 0 && e.sphere_top.w == 184);
  assert(g.sphere_top.x == 48 && g.sphere_top.y == 6 && g.sphere_top.w == 164);
  assert(e.sphere_bottom.y == 192 && g.sphere_bottom.y == 212 && g.sphere_bottom.w == 166);
  assert(e.about_title.y == 10 && e.portrait_y == 47 && e.about_name.y == 144);
  assert(e.about_version.y == 176 && e.about_hint.y == 203 && e.about_hint.x == 24);
  assert(e.save_box.x == 25 && e.save_box.y == 70 && e.save_line.y == 201);
  assert(e.fail_title_y == 25 && e.fail_body_y == 95);
  for (unsigned i = 0; i < sizeof SCREENS / sizeof *SCREENS; i++)
    check_screen(&SCREENS[i]);
  if (failures) {
    printf("%d layout checks failed\n", failures);
    return 1;
  }
  printf("layout: all screens pass\n");
  return 0;
}
