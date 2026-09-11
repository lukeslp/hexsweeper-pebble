// Luke Steuber. One state machine for touch and physical buttons.
#include "controller.h"
#include <string.h>
static void center(Controller *c, int cell) {
  c->selected = (uint8_t)cell;
  c->orientation = sphere_center_on(c->orientation, cell);
}
static void ring(Controller *c) {
  center(c, c->selected);
  c->neighbor_count =
      (uint8_t)sphere_neighbors(c->orientation, c->selected, c->neighbors);
  c->neighbor = 0;
}
void controller_init(Controller *c, uint32_t seed, Viewport view) {
  memset(c, 0, sizeof *c);
  c->viewport = view;
  c->orientation = sphere_identity();
  game_new(&c->game, seed);
  center(c, 0);
}
int controller_highlight(const Controller *c) {
  return c->mode == HS_EXPLORE ? c->neighbors[c->neighbor] : c->selected;
}
void controller_cancel(Controller *c) {
  if (c->gesture.drag)
    c->selected = (uint8_t)sphere_front(c->orientation);
  memset(&c->gesture, 0, sizeof c->gesture);
}
static void apply(Controller *c) {
  if (sphere_rotate(c->orientation, sphere_center(c->selected)).z < .6f) {
    center(c, c->selected);
    return;
  }
  if (c->flag)
    game_flag(&c->game, c->selected);
  else
    game_reveal(&c->game, c->selected);
}
void controller_button(Controller *c, int button) {
  if (c->gesture.active && button != HS_BACK)
    return;
  controller_cancel(c);
  if (c->mode == HS_PLAY) {
    if (button == HS_BACK) {
      c->mode = HS_PAUSE;
      c->menu = 0;
    } else if (button == HS_UP) {
      c->mode = HS_EXPLORE;
      ring(c);
    } else if (button == HS_DOWN)
      c->flag = !c->flag;
    else if (button == HS_SELECT)
      apply(c);
    else if (button == HS_HOLD_SELECT)
      game_flag(&c->game, c->selected);
  } else if (c->mode == HS_EXPLORE) {
    if (button == HS_BACK)
      c->mode = HS_PLAY;
    else if (button == HS_UP)
      c->neighbor = (c->neighbor + c->neighbor_count - 1) % c->neighbor_count;
    else if (button == HS_DOWN)
      c->neighbor = (c->neighbor + 1) % c->neighbor_count;
    else if (button == HS_SELECT) {
      center(c, c->neighbors[c->neighbor]);
      ring(c);
    }
  } else if (c->mode == HS_PAUSE) {
    if (button == HS_UP)
      c->menu = (c->menu + 5) % 6;
    else if (button == HS_DOWN)
      c->menu = (c->menu + 1) % 6;
    else if (button == HS_BACK)
      c->mode = HS_PLAY;
    else if (button == HS_SELECT) {
      if (c->menu == 0)
        c->mode = HS_PLAY;
      else if (c->menu == 1)
        c->direct = !c->direct;
      else if (c->menu == 2) {
        c->mode = HS_HELP;
        c->help = 0;
      } else if (c->menu == 3) {
        c->mode = HS_CONFIRM;
        c->menu = 0;
      }
      // Game modes (4) and About (5) are handled by the native shell.
    }
  } else if (c->mode == HS_HELP) {
    if (button == HS_UP)
      c->help = (c->help + 3) % 4;
    else if (button == HS_DOWN)
      c->help = (c->help + 1) % 4;
    else if (button == HS_BACK || button == HS_SELECT)
      c->mode = HS_PLAY;
  } else if (c->mode == HS_CONFIRM) {
    if (button == HS_BACK)
      c->mode = HS_PAUSE;
    else if (button == HS_UP || button == HS_DOWN)
      c->menu = !c->menu;
    else if (button == HS_SELECT) {
      if (c->menu) {
        uint8_t direct = c->direct;
        Viewport view = c->viewport;
        uint32_t seed = c->game.seed + 0x9e3779b9u;
        controller_init(c, seed, view);
        c->direct = direct;
      } else
        c->mode = HS_PLAY;
    }
  }
}
void controller_touch(Controller *c, int type, float x, float y, uint32_t ms,
                      bool non_navigational) {
  if (non_navigational || c->mode != HS_PLAY) {
    controller_cancel(c);
    return;
  }
  if (type == HS_TOUCH_DOWN) {
    controller_cancel(c);
    int cell = sphere_pick(c->orientation, c->viewport, x, y);
    if (cell < 0)
      return;
    c->gesture = (Gesture){.active = true,
                           .x = x,
                           .y = y,
                           .time = ms,
                           .cell = cell,
                           .orientation = c->orientation};
    return;
  }
  if (!c->gesture.active)
    return;
  float dx = x - c->gesture.x, dy = y - c->gesture.y;
  if (dx * dx + dy * dy > 64)
    c->gesture.drag = true;
  if (c->gesture.drag) {
    Vec from = sphere_arcball(c->viewport, c->gesture.x, c->gesture.y),
        to = sphere_arcball(c->viewport, x, y);
    c->orientation =
        sphere_multiply(sphere_between(from, to), c->gesture.orientation);
  }
  if (type != HS_TOUCH_UP)
    return;
  if (c->gesture.drag)
    c->selected = (uint8_t)sphere_front(c->orientation);
  else if (ms - c->gesture.time <= 350) {
    int cell = sphere_pick(c->orientation, c->viewport, x, y);
    if (cell >= 0 && cell == c->gesture.cell) {
      float z = sphere_rotate(c->orientation, sphere_center(cell)).z;
      center(c, cell);
      if (c->direct && z >= .6f)
        apply(c);
    }
  }
  memset(&c->gesture, 0, sizeof c->gesture);
}
static uint32_t checksum(const SavedGame *s) {
  const uint8_t *p = (const uint8_t *)s;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(SavedGame, checksum); i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
void controller_save(const Controller *c, SavedGame *out) {
  memset(out, 0, sizeof *out);
  out->magic = 0x48535048;
  out->version = 1;
  out->game = c->game;
  out->orientation = c->orientation;
  out->selected = c->selected;
  out->direct = c->direct;
  out->flag = c->flag;
  out->checksum = checksum(out);
}
bool controller_restore(Controller *c, const SavedGame *in, size_t size) {
  if (size != sizeof *in || in->magic != 0x48535048 || in->version != 1 ||
      in->checksum != checksum(in) || !game_valid(&in->game) ||
      !sphere_quat_valid(in->orientation) || in->selected >= HS_CELLS ||
      in->direct > 1 || in->flag > 1)
    return false;
  c->game = in->game;
  c->orientation = in->orientation;
  c->selected = in->selected;
  c->direct = in->direct;
  c->flag = in->flag;
  c->mode = HS_PLAY;
  controller_cancel(c);
  if (sphere_rotate(c->orientation, sphere_center(c->selected)).z < .6f)
    c->selected = (uint8_t)sphere_front(c->orientation);
  return true;
}
