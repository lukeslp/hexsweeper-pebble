// Luke Steuber. Finite Minesweeper over the flat hexagon adjacency.
#include "classic.h"
#include "classic_geometry.h"
#include <string.h>
static uint32_t random_next(uint32_t *s) {
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *s = x;
}
void classic_new(ClassicGame *g, uint32_t seed) {
  memset(g, 0, sizeof *g);
  g->seed = seed ? seed : 0x73bd921u;
  g->first = 255;
}
uint8_t classic_clue(const ClassicGame *g, int cell) {
  if (cell < 0 || cell >= FS_CELLS)
    return 0;
  uint8_t n = 0;
  for (int j = 0; j < FS_MESH[cell].degree; j++)
    n += (g->cells[FS_MESH[cell].neighbors[j]] & FS_MINE) != 0;
  return n;
}
int classic_open_count(const ClassicGame *g) {
  int n = 0;
  for (int i = 0; i < FS_CELLS; i++)
    n += (g->cells[i] & FS_OPEN) != 0 && !(g->cells[i] & FS_MINE);
  return n;
}
int classic_flag_count(const ClassicGame *g) {
  int n = 0;
  for (int i = 0; i < FS_CELLS; i++)
    n += (g->cells[i] & FS_FLAG) != 0;
  return n;
}
static void place(ClassicGame *g, int first) {
  bool excluded[FS_CELLS] = {false};
  excluded[first] = true;
  for (int j = 0; j < FS_MESH[first].degree; j++)
    excluded[FS_MESH[first].neighbors[j]] = true;
  uint8_t available[FS_CELLS];
  int n = 0;
  for (int i = 0; i < FS_CELLS; i++)
    if (!excluded[i])
      available[n++] = (uint8_t)i;
  for (int i = n - 1; i > 0; i--) {
    int j = (int)(random_next(&g->seed) % (uint32_t)(i + 1));
    uint8_t t = available[i];
    available[i] = available[j];
    available[j] = t;
  }
  for (int i = 0; i < FS_MINES; i++)
    g->cells[available[i]] |= FS_MINE;
  g->first = (uint8_t)first;
  g->status = FS_ACTIVE;
}
bool classic_flag(ClassicGame *g, int cell) {
  if (cell < 0 || cell >= FS_CELLS || g->status >= FS_WON ||
      (g->cells[cell] & FS_OPEN))
    return false;
  g->cells[cell] ^= FS_FLAG;
  return true;
}
bool classic_chord(ClassicGame *g, int cell) {
  if (cell < 0 || cell >= FS_CELLS || g->status != FS_ACTIVE ||
      !(g->cells[cell] & FS_OPEN))
    return false;
  int flags = 0;
  for (int j = 0; j < FS_MESH[cell].degree; j++)
    flags += (g->cells[FS_MESH[cell].neighbors[j]] & FS_FLAG) != 0;
  if (flags != classic_clue(g, cell))
    return false;
  bool changed = false;
  for (int j = 0; j < FS_MESH[cell].degree && g->status == FS_ACTIVE; j++) {
    int n = FS_MESH[cell].neighbors[j];
    // Never recurse through an open clue. Stop at the first mine so loss saves
    // retain exactly one detonated cell, as in an ordinary dig.
    if (!(g->cells[n] & (FS_OPEN | FS_FLAG)))
      changed |= classic_reveal(g, n);
  }
  return changed;
}
bool classic_reveal(ClassicGame *g, int cell) {
  if (cell < 0 || cell >= FS_CELLS || g->status >= FS_WON ||
      (g->cells[cell] & FS_FLAG))
    return false;
  if (g->cells[cell] & FS_OPEN)
    return classic_chord(g, cell);
  if (g->status == FS_READY)
    place(g, cell);
  g->cells[cell] |= FS_OPEN;
  if (g->cells[cell] & FS_MINE) {
    g->status = FS_LOST;
    return true;
  }
  uint8_t queue[FS_CELLS];
  int head = 0, tail = 0;
  queue[tail++] = (uint8_t)cell;
  while (head < tail) {
    int c = queue[head++];
    if (classic_clue(g, c))
      continue;
    for (int j = 0; j < FS_MESH[c].degree; j++) {
      int n = FS_MESH[c].neighbors[j];
      if (!(g->cells[n] & (FS_MINE | FS_OPEN | FS_FLAG))) {
        g->cells[n] |= FS_OPEN;
        queue[tail++] = (uint8_t)n;
      }
    }
  }
  if (classic_open_count(g) == FS_CELLS - FS_MINES)
    g->status = FS_WON;
  return true;
}
bool classic_valid(const ClassicGame *g) {
  if (!g->seed || g->status > FS_LOST)
    return false;
  int mines = 0, blasts = 0;
  for (int i = 0; i < FS_CELLS; i++) {
    uint8_t c = g->cells[i];
    if (c & ~7u || ((c & FS_OPEN) && (c & FS_FLAG)))
      return false;
    mines += (c & FS_MINE) != 0;
    blasts += ((c & (FS_MINE | FS_OPEN)) == (FS_MINE | FS_OPEN));
  }
  int opened = classic_open_count(g);
  if (g->status == FS_READY)
    return mines == 0 && opened == 0 && blasts == 0 && g->first == 255;
  if (mines != FS_MINES || g->first >= FS_CELLS ||
      !(g->cells[g->first] & FS_OPEN) || (g->cells[g->first] & FS_MINE) ||
      classic_clue(g, g->first))
    return false;
  if (g->status == FS_LOST)
    return blasts == 1 && opened < FS_CELLS - FS_MINES;
  if (blasts)
    return false;
  return g->status == FS_WON ? opened == FS_CELLS - FS_MINES
                             : opened > 0 && opened < FS_CELLS - FS_MINES;
}
