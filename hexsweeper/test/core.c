// Luke Steuber. Exercise production rules, projection and input state machine.
#include "compat.h"
#include "controller.h"
#include "geometry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Viewport VIEW = {100, 105, 86};
static void rules(void) {
  for (int first = 0; first < HS_CELLS; first++)
    for (uint32_t seed = 1; seed <= 1000; seed++) {
      Game g;
      game_new(&g, seed);
      assert(game_valid(&g));
      game_flag(&g, first);
      assert(!game_reveal(&g, first));
      assert(g.status == HS_READY);
      game_flag(&g, first);
      assert(game_reveal(&g, first));
      assert(game_valid(&g));
      assert(game_clue(&g, first) == 0);
      assert(!(g.cells[first] & HS_MINE));
      int mines = 0;
      for (int i = 0; i < HS_CELLS; i++)
        mines += (g.cells[i] & HS_MINE) != 0;
      assert(mines == 7);
      for (int n = 0; n < HS_MESH[first].degree; n++)
        assert(!(g.cells[HS_MESH[first].neighbors[n]] & HS_MINE));
      Game lost = g;
      for (int i = 0; i < HS_CELLS; i++)
        if (lost.cells[i] & HS_MINE) {
          game_reveal(&lost, i);
          break;
        }
      assert(lost.status == (g.status == HS_WON ? HS_WON : HS_LOST) &&
             game_valid(&lost));
      Game end = lost;
      assert(!game_flag(&lost, 0));
      assert(!game_reveal(&lost, 0));
      assert(!memcmp(&end, &lost, sizeof end));
      for (int i = 0; i < HS_CELLS; i++)
        if (!(g.cells[i] & HS_MINE))
          game_reveal(&g, i);
      assert(g.status == HS_WON && game_open_count(&g) == 35 && game_valid(&g));
    }
  Game g;
  game_new(&g, 42);
  int neighbor = HS_MESH[0].neighbors[0];
  game_flag(&g, neighbor);
  game_reveal(&g, 0);
  assert(g.cells[neighbor] == HS_FLAG);
  game_flag(&g, neighbor);
  game_reveal(&g, neighbor);
  assert(g.cells[neighbor] & HS_OPEN);
  g.cells[0] = HS_OPEN | HS_FLAG;
  assert(!game_valid(&g));
}
static void geometry(void) {
  Quat q = sphere_identity();
  int pentagons = 0;
  for (int i = 0; i < HS_CELLS; i++) {
    pentagons += HS_MESH[i].degree == 5;
    q = sphere_center_on(q, i);
    assert(sphere_quat_valid(q));
    assert(sphere_front(q) == i);
    assert(sphere_pick(q, VIEW, VIEW.x, VIEW.y) == i);
    assert(sphere_rotate(q, sphere_center(i)).z > .999f);
    uint8_t ns[6];
    int count = sphere_neighbors(q, i, ns);
    assert(count == HS_MESH[i].degree);
    for (int j = 0; j < count; j++) {
      int reciprocal = 0;
      for (int k = 0; k < HS_MESH[ns[j]].degree; k++)
        reciprocal += HS_MESH[ns[j]].neighbors[k] == i;
      assert(reciprocal == 1);
      for (int k = 0; k < j; k++)
        assert(ns[k] != ns[j]);
    }
    for (int j = 0; j < HS_CELLS; j++) {
      Polygon p;
      sphere_polygon(q, VIEW, j, &p);
      assert(p.count >= 0 && p.count <= 8);
      if (sphere_rotate(q, sphere_center(j)).z < -.5f)
        assert(p.count == 0);
      for (int k = 0; k < p.count; k++) {
        float x = (p.p[k].x - VIEW.x) / VIEW.r,
              y = (p.p[k].y - VIEW.y) / VIEW.r;
        assert(x * x + y * y < 1.001f);
      }
    }
    assert(sphere_pick(q, VIEW, 0, 0) == -1);
  }
  assert(pentagons == 12);
  Vec back = sphere_rotate(sphere_between((Vec){0, 0, 1}, (Vec){0, 0, -1}),
                           (Vec){0, 0, 1});
  assert(back.z < -.999f);
  for (int i = 0; i < 10000; i++)
    q = sphere_center_on(q, i % HS_CELLS);
  assert(sphere_quat_valid(q));
}
static void tap(Controller *c, float x, float y, uint32_t t) {
  controller_touch(c, HS_TOUCH_DOWN, x, y, t, false);
  controller_touch(c, HS_TOUCH_UP, x, y, t + 100, false);
}
static void input(void) {
  Controller c;
  controller_init(&c, 123456, VIEW);
  tap(&c, 100, 105, 0);
  assert(c.game.status == HS_READY);
  c.direct = 1;
  controller_touch(&c, HS_TOUCH_DOWN, 100, 105, 1000, false);
  controller_touch(&c, HS_TOUCH_MOVE, 135, 105, 1040, false);
  controller_button(&c, HS_BACK);
  assert(c.mode == HS_PAUSE && !c.gesture.active);
  controller_touch(&c, HS_TOUCH_UP, 135, 105, 1080, false);
  assert(c.game.status == HS_READY);
  controller_button(&c, HS_BACK);
  controller_button(&c, HS_DOWN);
  tap(&c, VIEW.x, VIEW.y, 1100);
  assert(game_flag_count(&c.game) == 1 && c.game.status == HS_READY);
  tap(&c, VIEW.x, VIEW.y, 1220);
  assert(game_flag_count(&c.game) == 0);
  controller_init(&c, 123456, VIEW);
  controller_button(&c, HS_HOLD_SELECT);
  assert(c.game.cells[0] == HS_FLAG && c.game.status == HS_READY);
  controller_button(&c, HS_SELECT);
  assert(c.game.status == HS_READY);
  controller_button(&c, HS_HOLD_SELECT);
  controller_button(&c, HS_SELECT);
  assert(c.game.status == HS_ACTIVE);
  controller_init(&c, 123456, VIEW);
  c.direct = 1;
  controller_touch(&c, HS_TOUCH_DOWN, 100, 105, 0, false);
  controller_touch(&c, HS_TOUCH_MOVE, 150, 105, 80, false);
  controller_touch(&c, HS_TOUCH_MOVE, 100, 105, 120, false);
  controller_touch(&c, HS_TOUCH_UP, 100, 105, 180, false);
  assert(c.game.status == HS_READY && !c.gesture.active);
  controller_touch(&c, HS_TOUCH_DOWN, 100, 105, 200, false);
  controller_touch(&c, HS_TOUCH_UP, 100, 105, 600, false);
  assert(c.game.status == HS_READY);
  controller_touch(&c, HS_TOUCH_DOWN, 100, 105, 700, false);
  controller_cancel(&c);
  controller_touch(&c, HS_TOUCH_UP, 100, 105, 800, false);
  assert(c.game.status == HS_READY);
  controller_touch(&c, HS_TOUCH_DOWN, 100, 105, 900, true);
  controller_touch(&c, HS_TOUCH_UP, 100, 105, 1000, true);
  assert(c.game.status == HS_READY);
  bool rim = false;
  for (int i = 0; i < HS_CELLS; i++) {
    Vec p = sphere_rotate(c.orientation, sphere_center(i));
    if (p.z > .2f && p.z < .6f) {
      tap(&c, VIEW.x + VIEW.r * p.x, VIEW.y - VIEW.r * p.y, 1100);
      assert(c.selected == i && c.game.status == HS_READY);
      rim = true;
      break;
    }
  }
  assert(rim);
  tap(&c, VIEW.x, VIEW.y, 1300);
  assert(c.game.status == HS_ACTIVE);
  controller_button(&c, HS_BACK);
  assert(c.mode == HS_PAUSE);
  SavedGame before, after;
  controller_save(&c, &before);
  tap(&c, 100, 105, 1600);
  controller_save(&c, &after);
  assert(!memcmp(&before, &after, sizeof before));
  for (int i = 0; i < 3; i++)
    controller_button(&c, HS_DOWN);
  controller_button(&c, HS_SELECT);
  assert(c.mode == HS_CONFIRM && c.menu == 0);
  controller_button(&c, HS_SELECT);
  controller_save(&c, &after);
  assert(!memcmp(&before, &after, sizeof before));
  controller_button(&c, HS_BACK);
  for (int i = 0; i < 3; i++)
    controller_button(&c, HS_DOWN);
  controller_button(&c, HS_SELECT);
  controller_button(&c, HS_DOWN);
  controller_button(&c, HS_SELECT);
  assert(c.mode == HS_PLAY && c.game.status == HS_READY && c.direct == 1);
  // Navigate every graph edge using only the production Explore controls.
  bool seen[42] = {false};
  Controller queue[42];
  int head = 0, tail = 0;
  queue[tail++] = c;
  seen[c.selected] = true;
  while (head < tail) {
    Controller current = queue[head++];
    controller_button(&current, HS_UP);
    assert(current.mode == HS_EXPLORE);
    for (int j = 0; j < current.neighbor_count; j++) {
      Controller step = current;
      int target = controller_highlight(&step);
      controller_button(&step, HS_SELECT);
      assert(step.selected == target && step.mode == HS_EXPLORE);
      assert(sphere_rotate(step.orientation, sphere_center(target)).z > .999f);
      controller_button(&step, HS_BACK);
      assert(step.mode == HS_PLAY);
      if (!seen[target]) {
        seen[target] = true;
        queue[tail++] = step;
      }
      controller_button(&current, HS_DOWN);
    }
  }
  assert(tail == 42);
}
static void persistence(void) {
  Controller c, d;
  controller_init(&c, 128, VIEW);
  controller_init(&d, 1, VIEW);
  controller_button(&c, HS_SELECT);
  c.direct = 1;
  c.flag = 1;
  c.selected = 7;
  c.orientation = sphere_center_on(c.orientation, 7);
  SavedGame save;
  controller_save(&c, &save);
  assert(controller_restore(&d, &save, sizeof save));
  assert(!memcmp(&c.game, &d.game, sizeof c.game) && c.selected == d.selected &&
         d.direct && d.flag);
  assert(!controller_restore(&d, &save, sizeof save - 1));
  for (size_t i = 0; i < sizeof save; i++) {
    SavedGame bad = save;
    ((uint8_t *)&bad)[i] ^= 1;
    assert(!controller_restore(&d, &bad, sizeof bad));
  }
  c.orientation.w = NAN;
  controller_save(&c, &save);
  assert(!controller_restore(&d, &save, sizeof save));
}
int main(void) {
  assert(!firmware_allows_touch(3, 99, 255));
  assert(!firmware_allows_touch(4, 9, 169));
  assert(!firmware_allows_touch(4, 17, 0));
  assert(!firmware_allows_touch(4, 32, 255));
  assert(!firmware_allows_touch(4, 33, 0));
  assert(!firmware_allows_touch(4, 33, 1));
  assert(firmware_allows_touch(4, 33, 2));
  assert(firmware_allows_touch(4, 37, 0));
  assert(firmware_allows_touch(5, 0, 0));
  rules();
  geometry();
  input();
  persistence();
  puts("PASS: 42,000 boards; wins/losses/flags; all 42 button-reachable cells; "
       "touch arbitration; clipping; saves");
}
