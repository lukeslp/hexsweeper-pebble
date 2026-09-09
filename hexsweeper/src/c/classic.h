#ifndef FS_GAME_H
#define FS_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define FS_CELLS 37
#define FS_MINES 6
enum { FS_MINE = 1, FS_OPEN = 2, FS_FLAG = 4 };
enum { FS_READY, FS_ACTIVE, FS_WON, FS_LOST };
typedef struct {
  uint32_t seed;
  uint8_t cells[FS_CELLS], status, first;
} ClassicGame;
void classic_new(ClassicGame *g, uint32_t seed);
uint8_t classic_clue(const ClassicGame *g, int cell);
int classic_open_count(const ClassicGame *g);
int classic_flag_count(const ClassicGame *g);
bool classic_chord(ClassicGame *g, int cell);
bool classic_reveal(ClassicGame *g, int cell);
bool classic_flag(ClassicGame *g, int cell);
bool classic_valid(const ClassicGame *g);
#endif
