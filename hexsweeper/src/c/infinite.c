// Luke Steuber. Mine layout never depends on exploration order.
#include "infinite.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(Infinite) <= 1800,
               "Two snapshots must fit conservative watch storage");
static int32_t div8(int32_t x) {
  return (int32_t)((int64_t)x >= 0 ? x / 8 : ((int64_t)x - 7) / 8);
}
static int chunk_at(const Infinite *g, InfCoord p) {
  int32_t q = div8(p.q), r = div8(p.r);
  for (int i = 0; i < g->count; i++)
    if (g->chunks[i].q == q && g->chunks[i].r == r)
      return i;
  return -1;
}
static uint64_t bit(InfCoord p) {
  int x = (int64_t)p.q - (int64_t)div8(p.q) * 8;
  int y = (int64_t)p.r - (int64_t)div8(p.r) * 8;
  return (uint64_t)1 << (y * 8 + x);
}
static int ensure(Infinite *g, InfCoord p) {
  int i = chunk_at(g, p);
  if (i >= 0)
    return i;
  if (g->count == INF_CHUNKS) {
    g->full = 1;
    return -1;
  }
  i = g->count++;
  g->chunks[i] = (InfChunk){.q = div8(p.q), .r = div8(p.r)};
  g->full = 0;
  return i;
}
void inf_new(Infinite *g, uint32_t seed) {
  memset(g, 0, sizeof *g);
  g->seed = seed ? seed : 1;
}
bool inf_neighbor(InfCoord p, int d, InfCoord *out) {
  const int dq[] = {1, 0, -1, -1, 0, 1}, dr[] = {0, 1, 1, 0, -1, -1};
  if (d < 0 || d > 5)
    return false;
  int64_t q = (int64_t)p.q + dq[d], r = (int64_t)p.r + dr[d];
  if (q < INT32_MIN || q > INT32_MAX || r < INT32_MIN || r > INT32_MAX)
    return false;
  *out = (InfCoord){q, r};
  return true;
}
static uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  return x ^ (x >> 16);
}
bool inf_mine(const Infinite *g, InfCoord p) {
  if (g->status != INF_READY) {
    int64_t q = (int64_t)p.q - g->first.q, r = (int64_t)p.r - g->first.r;
    if (llabs(q) <= 1 && llabs(r) <= 1 && llabs(q + r) <= 1)
      return false;
  }
  return mix(g->seed ^ mix((uint32_t)p.q + 0x9e3779b9U) ^
             mix((uint32_t)p.r + 0x85ebca6bU)) %
             6 ==
         0;
}
int inf_clue(const Infinite *g, InfCoord p) {
  int n = 0;
  InfCoord near;
  for (int d = 0; d < 6; d++)
    if (inf_neighbor(p, d, &near))
      n += inf_mine(g, near);
  return n;
}
int inf_state(const Infinite *g, InfCoord p) {
  int i = chunk_at(g, p);
  if (i < 0)
    return 0;
  uint64_t mask = bit(p);
  return ((g->chunks[i].open & mask) ? INF_OPEN : 0) |
         ((g->chunks[i].flags & mask) ? INF_FLAG : 0);
}
bool inf_flag(Infinite *g, InfCoord p) {
  if (g->status == INF_LOST || (inf_state(g, p) & INF_OPEN))
    return false;
  int i = ensure(g, p);
  if (i < 0)
    return false;
  uint64_t mask = bit(p);
  if (g->chunks[i].flags & mask)
    g->flagged--;
  else
    g->flagged++;
  g->chunks[i].flags ^= mask;
  if (!g->chunks[i].flags && !g->chunks[i].open) {
    g->chunks[i] = g->chunks[--g->count];
    memset(&g->chunks[g->count], 0, sizeof g->chunks[0]);
    g->scan = 0;
    g->again = 1;
    g->full = 0;
  }
  return true;
}
static bool open_one(Infinite *g, InfCoord p) {
  if (inf_state(g, p))
    return false;
  int i = ensure(g, p);
  if (i < 0)
    return false;
  if (g->status == INF_READY) {
    g->first = p;
    g->status = INF_ACTIVE;
  }
  g->chunks[i].open |= bit(p);
  if (inf_mine(g, p)) {
    g->status = INF_LOST;
    g->busy = 0;
    return true;
  }
  g->opened++;
  if (!inf_clue(g, p)) {
    g->busy = 1;
    g->again = 1;
  }
  return true;
}
bool inf_reveal(Infinite *g, InfCoord p) {
  if (g->status == INF_LOST || (inf_state(g, p) & INF_FLAG))
    return false;
  if (!(inf_state(g, p) & INF_OPEN)) {
    bool changed = open_one(g, p);
    g->scan = 0;
    return changed;
  }
  int flags = 0;
  InfCoord near;
  for (int d = 0; d < 6; d++)
    if (inf_neighbor(p, d, &near))
      flags += (inf_state(g, near) & INF_FLAG) != 0;
  if (flags != inf_clue(g, p))
    return false;
  bool changed = false;
  for (int d = 0; d < 6 && g->status != INF_LOST; d++)
    if (inf_neighbor(p, d, &near))
      changed |= open_one(g, near);
  g->scan = 0;
  return changed;
}
bool inf_pump(Infinite *g, unsigned budget) {
  bool changed = false;
  while (g->busy && budget-- && g->status == INF_ACTIVE) {
    if (g->scan >= g->count * 64) {
      g->scan = 0;
      if (!g->again) {
        g->busy = 0;
        break;
      }
      g->again = 0;
    }
    if (!g->count) {
      g->busy = 0;
      break;
    }
    int index = g->scan++, c = index / 64, k = index % 64;
    if (!(g->chunks[c].open & ((uint64_t)1 << k)))
      continue;
    InfCoord p = {(int64_t)g->chunks[c].q * 8 + k % 8,
                  (int64_t)g->chunks[c].r * 8 + k / 8};
    if (inf_clue(g, p))
      continue;
    for (int d = 0; d < 6; d++) {
      InfCoord near;
      if (inf_neighbor(p, d, &near))
        changed |= open_one(g, near);
    }
  }
  return changed;
}
bool inf_valid(const Infinite *g) {
  if (!g->seed || g->count > INF_CHUNKS || g->status > INF_LOST ||
      g->busy > 1 || g->again > 1 || g->full > 1 || g->scan > g->count * 64)
    return false;
  unsigned opened = 0, flags = 0, blasts = 0;
  for (int i = 0; i < g->count; i++) {
    const InfChunk *c = &g->chunks[i];
    if (c->q < INT32_MIN / 8 || c->q > INT32_MAX / 8 || c->r < INT32_MIN / 8 ||
        c->r > INT32_MAX / 8 || (c->open & c->flags))
      return false;
    for (int j = 0; j < i; j++)
      if (c->q == g->chunks[j].q && c->r == g->chunks[j].r)
        return false;
    for (int k = 0; k < 64; k++) {
      uint64_t mask = (uint64_t)1 << k;
      flags += (c->flags & mask) != 0;
      if (c->open & mask) {
        InfCoord p = {(int64_t)c->q * 8 + k % 8, (int64_t)c->r * 8 + k / 8};
        if (inf_mine(g, p))
          blasts++;
        else
          opened++;
      }
    }
  }
  if (opened != g->opened || flags != g->flagged)
    return false;
  if (g->status == INF_READY)
    return !opened && !blasts && !g->busy;
  if (!(inf_state(g, g->first) & INF_OPEN) || inf_clue(g, g->first))
    return false;
  return g->status == INF_LOST ? blasts == 1 && !g->busy
                               : blasts == 0 && opened > 0;
}
