// One optional screen; no game state or persistence. Luke Steuber.
#include "about.h"
#include "ui_style.h"
static GBitmap *portrait;
void about_open(void) {
  if (!portrait)
    portrait = gbitmap_create_with_resource(RESOURCE_ID_ABOUT_PORTRAIT);
}
void about_close(void) {
  if (portrait) {
    gbitmap_destroy(portrait);
    portrait = NULL;
  }
}
static void line(GContext *ctx, const char *value, int width, int y,
                 int height, const char *font) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, value, fonts_get_system_font(font),
      GRect(24, y, width - 48, height), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentCenter, NULL);
}
void about_draw(GContext *ctx, int width) {
  line(ctx, "About", width, 10, 37, UI_FONT_TITLE);
  if (portrait) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, portrait, GRect((width - 96) / 2, 47, 96, 96));
  }
  line(ctx, "Luke Steuber", width, 144, 32, UI_FONT_BODY);
  line(ctx, "Version " UI_APP_VERSION, width, 176, 25, UI_FONT_SMALL);
  line(ctx, "Tap / Select / Back", width, 203, 25, UI_FONT_SMALL);
}
