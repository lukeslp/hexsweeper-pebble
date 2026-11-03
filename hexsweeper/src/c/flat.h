// Luke Steuber. Classic and Infinite views inside the shared watch app.
#pragma once
#include <pebble.h>
void flat_init(int kind, int width, int height);
void flat_draw(GContext *);
int flat_button(int button);
#ifdef PBL_TOUCH
// The shell supplies elapsed tap age from its AppTimer, never the wall clock.
int flat_touch(const TouchEvent *, uint32_t tap_age);
#endif
bool flat_save(void);
bool flat_busy(void);
void flat_tick(void);
void flat_cancel(void);
void flat_trace(void);
