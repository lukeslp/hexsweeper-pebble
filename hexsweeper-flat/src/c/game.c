// Luke Steuber. Finite Minesweeper over the flat hexagon adjacency.
#include "game.h"
#include "geometry.h"
#include <string.h>
static uint32_t random_next(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *s = x;
}
void game_new(Game *g, uint32_t seed) {
  memset(g, 0, sizeof *g);
  g->seed = seed ? seed : 0x73bd921u;
  g->first = 255;
}
uint8_t game_clue(const Game *g, int cell) {
  if (cell < 0 || cell >= HS_CELLS)
    return 0;
  uint8_t n = 0;
  for (int j = 0; j < HS_MESH[cell].degree; j++)
    n += (g->cells[HS_MESH[cell].neighbors[j]] & HS_MINE) != 0;
  return n;
}
int game_open_count(const Game *g) {
  int n = 0;
  for (int i = 0; i < HS_CELLS; i++)
    n += (g->cells[i] & HS_OPEN) != 0 && !(g->cells[i] & HS_MINE);
  return n;
}
int game_flag_count(const Game *g) {
  int n = 0;
  for (int i = 0; i < HS_CELLS; i++)
    n += (g->cells[i] & HS_FLAG) != 0;
  return n;
}
static void place(Game *g, int first) {
  bool excluded[HS_CELLS] = {false};
  excluded[first] = true;
  for (int j = 0; j < HS_MESH[first].degree; j++)
    excluded[HS_MESH[first].neighbors[j]] = true;
  uint8_t available[HS_CELLS];
  int n = 0;
  for (int i = 0; i < HS_CELLS; i++)
    if (!excluded[i])
      available[n++] = (uint8_t)i;
  for (int i = n - 1; i > 0; i--) {
    int j = (int)(random_next(&g->seed) % (uint32_t)(i + 1));
    uint8_t t = available[i];
    available[i] = available[j];
    available[j] = t;
  }
  for (int i = 0; i < HS_MINES; i++)
    g->cells[available[i]] |= HS_MINE;
  g->first = (uint8_t)first;
  g->status = HS_ACTIVE;
}
bool game_flag(Game *g, int cell) {
  if (cell < 0 || cell >= HS_CELLS || g->status >= HS_WON ||
      (g->cells[cell] & HS_OPEN))
    return false;
  g->cells[cell] ^= HS_FLAG;
  return true;
}
bool game_chord(Game *g, int cell) {
  if (cell < 0 || cell >= HS_CELLS || g->status != HS_ACTIVE ||
      !(g->cells[cell] & HS_OPEN))
    return false;
  int flags = 0;
  for (int j = 0; j < HS_MESH[cell].degree; j++)
    flags += (g->cells[HS_MESH[cell].neighbors[j]] & HS_FLAG) != 0;
  if (flags != game_clue(g, cell))
    return false;
  bool changed = false;
  for (int j = 0; j < HS_MESH[cell].degree && g->status == HS_ACTIVE; j++) {
    int n = HS_MESH[cell].neighbors[j];
    // Never recurse through an open clue. Stop at the first mine so loss saves
    // retain exactly one detonated cell, as in an ordinary dig.
    if (!(g->cells[n] & (HS_OPEN | HS_FLAG)))
      changed |= game_reveal(g, n);
  }
  return changed;
}
bool game_reveal(Game *g, int cell) {
  if (cell < 0 || cell >= HS_CELLS || g->status >= HS_WON ||
      (g->cells[cell] & HS_FLAG))
    return false;
  if (g->cells[cell] & HS_OPEN)
    return game_chord(g, cell);
  if (g->status == HS_READY)
    place(g, cell);
  g->cells[cell] |= HS_OPEN;
  if (g->cells[cell] & HS_MINE) {
    g->status = HS_LOST;
    return true;
  }
  uint8_t queue[HS_CELLS];
  int head = 0, tail = 0;
  queue[tail++] = (uint8_t)cell;
  while (head < tail) {
    int c = queue[head++];
    if (game_clue(g, c))
      continue;
    for (int j = 0; j < HS_MESH[c].degree; j++) {
      int n = HS_MESH[c].neighbors[j];
      if (!(g->cells[n] & (HS_MINE | HS_OPEN | HS_FLAG))) {
        g->cells[n] |= HS_OPEN;
        queue[tail++] = (uint8_t)n;
      }
    }
  }
  if (game_open_count(g) == HS_CELLS - HS_MINES)
    g->status = HS_WON;
  return true;
}
bool game_valid(const Game *g) {
  if (!g->seed || g->status > HS_LOST)
    return false;
  int mines = 0, blasts = 0;
  for (int i = 0; i < HS_CELLS; i++) {
    uint8_t c = g->cells[i];
    if (c & ~7u || ((c & HS_OPEN) && (c & HS_FLAG)))
      return false;
    mines += (c & HS_MINE) != 0;
    blasts += ((c & (HS_MINE | HS_OPEN)) == (HS_MINE | HS_OPEN));
  }
  int opened = game_open_count(g);
  if (g->status == HS_READY)
    return mines == 0 && opened == 0 && blasts == 0 && g->first == 255;
  if (mines != HS_MINES || g->first >= HS_CELLS ||
      !(g->cells[g->first] & HS_OPEN) || (g->cells[g->first] & HS_MINE) ||
      game_clue(g, g->first))
    return false;
  if (g->status == HS_LOST)
    return blasts == 1 && opened < HS_CELLS - HS_MINES;
  if (blasts)
    return false;
  return g->status == HS_WON ? opened == HS_CELLS - HS_MINES
                             : opened > 0 && opened < HS_CELLS - HS_MINES;
}
