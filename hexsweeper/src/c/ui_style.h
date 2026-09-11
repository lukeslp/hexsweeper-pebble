// Readable native watch type. Luke Steuber.
#pragma once
#define UI_FONT_SMALL FONT_KEY_GOTHIC_18_BOLD
#define UI_FONT_BODY FONT_KEY_GOTHIC_24_BOLD
#define UI_FONT_TITLE FONT_KEY_GOTHIC_28_BOLD
#define UI_ROW_HEIGHT 38
// Shared drawing and input geometry; the complete Classic board stays visible.
#define UI_HEX_X PBL_IF_ROUND_ELSE(32, 26)
#define UI_HEX_Y PBL_IF_ROUND_ELSE(27, 22)
#define UI_HEX_HALF_X (UI_HEX_X / 2)
#define UI_HEX_HALF_Y PBL_IF_ROUND_ELSE(18, 15)
#define UI_CONTENT_INSET PBL_IF_ROUND_ELSE(30, 8)
#define UI_APP_VERSION "0.7.1"
