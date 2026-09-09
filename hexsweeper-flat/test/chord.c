// Luke Steuber. Chording against generated, valid game states.
#include "game.h"
#include "geometry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  unsigned matched = 0, wrong = 0, degrees = 0, won = 0;
  for (unsigned seed = 1; seed <= 100; seed++) {
    for (int first = 0; first < HS_CELLS; first++) {
      Game initial;
      game_new(&initial, seed);
      assert(!game_chord(&initial, first));
      game_reveal(&initial, first);
      for (int cell = 0; cell < HS_CELLS; cell++) {
        if (initial.status != HS_ACTIVE || !(initial.cells[cell] & HS_OPEN) ||
            !game_clue(&initial, cell)) continue;
        int safe = -1, mine = -1;
        for (int j = 0; j < HS_MESH[cell].degree; j++) {
          int n = HS_MESH[cell].neighbors[j];
          if (initial.cells[n] & HS_MINE) mine = n;
          else if (!(initial.cells[n] & HS_OPEN)) safe = n;
        }
        if (safe < 0 || mine < 0) continue;
        Game g = initial, before = g;
        assert(!game_reveal(&g, cell)); // Too few flags must do nothing.
        assert(!memcmp(&g, &before, sizeof g));
        for (int j = 0; j < HS_MESH[cell].degree; j++) {
          int n = HS_MESH[cell].neighbors[j];
          if (g.cells[n] & HS_MINE) assert(game_flag(&g, n));
        }
        Game correct = g;
        assert(game_flag(&g, safe));
        before = g;
        assert(!game_chord(&g, cell)); // Too many flags also do nothing.
        assert(!memcmp(&g, &before, sizeof g));
        g = correct;
        assert(game_reveal(&g, cell));
        assert(game_valid(&g) && g.status != HS_LOST);
        for (int j = 0; j < HS_MESH[cell].degree; j++) {
          int n = HS_MESH[cell].neighbors[j];
          assert(g.cells[n] & ((g.cells[n] & HS_MINE) ? HS_FLAG : HS_OPEN));
        }
        matched++;
        degrees |= 1u << HS_MESH[cell].degree;
        g = correct;
        assert(game_flag(&g, mine) && game_flag(&g, safe));
        assert(game_reveal(&g, cell)); // Matching but misplaced flags lose.
        assert(g.status == HS_LOST && game_valid(&g));
        before = g;
        assert(!game_chord(&g, cell));
        assert(!memcmp(&g, &before, sizeof g));
        wrong++;
        // Leave just one safe cell covered: a chord can finish the board.
        g = correct;
        for (int n = 0; n < HS_CELLS; n++)
          if (n != safe && !(g.cells[n] & HS_MINE)) g.cells[n] |= HS_OPEN;
        assert(game_valid(&g));
        assert(game_reveal(&g, cell));
        assert(g.status == HS_WON && game_valid(&g));
        won++;
      }
    }
  }
  assert(matched && wrong && won);
  assert(degrees & (1u << 6));
#if HS_CELLS == 42
  assert(degrees & (1u << 5));
#endif
  printf("PASS %u matching, %u incorrect, %u winning chords; degree mask %u\n",
         matched, wrong, won, degrees);
}
