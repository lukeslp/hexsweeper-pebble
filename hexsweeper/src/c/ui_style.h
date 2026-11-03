// Readable native watch type and tile palette. Luke Steuber.
#pragma once
#include "layout.h"
#include <pebble.h>
#define UI_FONT_SMALL FONT_KEY_GOTHIC_18_BOLD
#define UI_FONT_BODY FONT_KEY_GOTHIC_24_BOLD
#define UI_FONT_TITLE FONT_KEY_GOTHIC_28_BOLD
#define UI_APP_VERSION "0.8.0"
// Gothic Bold by point size; nothing smaller than 18.
static inline GFont ui_font(int size) {
  return fonts_get_system_font(size >= 28   ? UI_FONT_TITLE
                               : size >= 24 ? UI_FONT_BODY
                                            : UI_FONT_SMALL);
}
static inline GRect ui_rect(Box b) { return GRect(b.x, b.y, b.w, b.h); }
// Tile states. Color watches keep the 0.7 palette. On black-and-white
// watches every state has its own shape: covered cells are black with a white
// edge and dots, open cells white with black clues, flags a white pennant on
// black, mines a spiked glyph, and the detonated mine is inverted.
#ifdef PBL_COLOR
#define UI_COVERED GColorDarkGray
#define UI_EDGE GColorLightGray
#define UI_FLAG_FILL GColorChromeYellow
#define UI_FLAG_MARK GColorBlack
#define UI_FOCUS_CORE GColorCyan
#define UI_MINE_FILL(blast) GColorMelon
#define UI_MINE_INK(blast) GColorBlack
static inline GColor ui_edge_for(GColor fill) {
  (void)fill;
  return GColorLightGray;
}
#else
#define UI_COVERED GColorBlack
#define UI_EDGE GColorWhite
#define UI_FLAG_FILL GColorBlack
#define UI_FLAG_MARK GColorWhite
#define UI_FOCUS_CORE GColorWhite
#define UI_MINE_FILL(blast) ((blast) ? GColorBlack : GColorWhite)
#define UI_MINE_INK(blast) ((blast) ? GColorWhite : GColorBlack)
static inline GColor ui_edge_for(GColor fill) {
  return gcolor_equal(fill, GColorWhite) ? GColorBlack : GColorWhite;
}
#endif
#ifdef PBL_TOUCH
#define UI_ABOUT_HINT "Tap / Select / Back"
#else
#define UI_ABOUT_HINT "Select or Back"
#endif
