// Luke Steuber. Classic and Infinite views inside the shared watch app.
#pragma once
#include <pebble.h>
void flat_init(int kind, int width, int height);
void flat_draw(GContext *);
int flat_button(int button);
int flat_touch(const TouchEvent *, uint32_t now);
bool flat_save(void);
bool flat_busy(void);
void flat_tick(void);
void flat_cancel(void);
void flat_trace(void);
