// Luke Steuber. Deterministic axial field with retained sparse exploration.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define INF_CHUNKS 64
enum { INF_OPEN = 1, INF_FLAG = 2 };
enum { INF_READY, INF_ACTIVE, INF_LOST };
typedef struct {
  int32_t q, r;
} InfCoord;
typedef struct {
  int32_t q, r;
  uint64_t open, flags;
} InfChunk;
typedef struct {
  uint32_t seed, opened, flagged;
  InfCoord first, focus;
  uint16_t scan;
  uint8_t count, status, busy, again, full;
  InfChunk chunks[INF_CHUNKS];
} Infinite;
void inf_new(Infinite *, uint32_t);
bool inf_neighbor(InfCoord, int, InfCoord *);
bool inf_mine(const Infinite *, InfCoord);
int inf_clue(const Infinite *, InfCoord);
int inf_state(const Infinite *, InfCoord);
bool inf_flag(Infinite *, InfCoord);
bool inf_reveal(Infinite *, InfCoord);
bool inf_pump(Infinite *, unsigned budget);
bool inf_valid(const Infinite *);
