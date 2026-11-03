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
static void line(GContext *ctx, const char *value, Box b, int font) {
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, value, ui_font(font), ui_rect(b),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}
void about_draw(GContext *ctx, int width) {
  const Layout *l = &ui_layout;
  line(ctx, "About", l->about_title, l->about_title_font);
  if (portrait) {
    // Each display ships a portrait sized for it; center whatever was loaded.
    GSize size = gbitmap_get_bounds(portrait).size;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, portrait,
                                 GRect((width - size.w) / 2, l->portrait_y, size.w, size.h));
  }
  line(ctx, "Luke Steuber", l->about_name, 24);
  line(ctx, "Version " UI_APP_VERSION, l->about_version, 18);
  line(ctx, UI_ABOUT_HINT, l->about_hint, 18);
}
