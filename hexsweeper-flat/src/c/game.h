#ifndef HS_GAME_H
#define HS_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define HS_CELLS 37
#define HS_MINES 6
enum { HS_MINE = 1, HS_OPEN = 2, HS_FLAG = 4 };
enum { HS_READY, HS_ACTIVE, HS_WON, HS_LOST };
typedef struct {
  uint32_t seed;
  uint8_t cells[HS_CELLS], status, first;
} Game;
void game_new(Game *g, uint32_t seed);
uint8_t game_clue(const Game *g, int cell);
int game_open_count(const Game *g);
int game_flag_count(const Game *g);
bool game_reveal(Game *g, int cell);
bool game_flag(Game *g, int cell);
bool game_valid(const Game *g);
#endif
