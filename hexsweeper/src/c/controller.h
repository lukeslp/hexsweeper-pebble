#ifndef HS_CONTROLLER_H
#define HS_CONTROLLER_H
#include "sphere.h"
#include <stddef.h>
enum { HS_PLAY, HS_EXPLORE, HS_PAUSE, HS_HELP, HS_CONFIRM };
enum { HS_UP, HS_DOWN, HS_SELECT, HS_HOLD_SELECT, HS_BACK };
enum { HS_TOUCH_DOWN, HS_TOUCH_MOVE, HS_TOUCH_UP };
typedef struct {
  bool active, drag;
  float x, y;
  uint32_t time;
  int cell;
  Quat orientation;
} Gesture;
typedef struct {
  Game game;
  Quat orientation;
  Viewport viewport;
  uint8_t selected, direct, flag, mode, menu, help;
  uint8_t neighbors[6], neighbor_count, neighbor;
  Gesture gesture;
} Controller;
typedef struct {
  uint32_t magic, version;
  Game game;
  Quat orientation;
  uint8_t selected, direct, flag, reserved;
  uint32_t checksum;
} SavedGame;
void controller_init(Controller *c, uint32_t seed, Viewport view);
void controller_button(Controller *c, int button);
void controller_touch(Controller *c, int type, float x, float y, uint32_t ms,
                      bool non_navigational);
void controller_cancel(Controller *c);
int controller_highlight(const Controller *c);
void controller_save(const Controller *c, SavedGame *out);
bool controller_restore(Controller *c, const SavedGame *in, size_t size);
#endif
