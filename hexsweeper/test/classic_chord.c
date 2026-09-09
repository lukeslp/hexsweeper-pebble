// Luke Steuber. Chording against generated, valid game states.
#include "classic.h"
#include "classic_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  unsigned matched = 0, wrong = 0, degrees = 0, won = 0;
  for (unsigned seed = 1; seed <= 100; seed++) {
    for (int first = 0; first < FS_CELLS; first++) {
      ClassicGame initial;
      classic_new(&initial, seed);
      assert(!classic_chord(&initial, first));
      classic_reveal(&initial, first);
      for (int cell = 0; cell < FS_CELLS; cell++) {
        if (initial.status != FS_ACTIVE || !(initial.cells[cell] & FS_OPEN) ||
            !classic_clue(&initial, cell)) continue;
        int safe = -1, mine = -1;
        for (int j = 0; j < FS_MESH[cell].degree; j++) {
          int n = FS_MESH[cell].neighbors[j];
          if (initial.cells[n] & FS_MINE) mine = n;
          else if (!(initial.cells[n] & FS_OPEN)) safe = n;
        }
        if (safe < 0 || mine < 0) continue;
        ClassicGame g = initial, before = g;
        assert(!classic_reveal(&g, cell)); // Too few flags must do nothing.
        assert(!memcmp(&g, &before, sizeof g));
        for (int j = 0; j < FS_MESH[cell].degree; j++) {
          int n = FS_MESH[cell].neighbors[j];
          if (g.cells[n] & FS_MINE) assert(classic_flag(&g, n));
        }
        ClassicGame correct = g;
        assert(classic_flag(&g, safe));
        before = g;
        assert(!classic_chord(&g, cell)); // Too many flags also do nothing.
        assert(!memcmp(&g, &before, sizeof g));
        g = correct;
        assert(classic_reveal(&g, cell));
        assert(classic_valid(&g) && g.status != FS_LOST);
        for (int j = 0; j < FS_MESH[cell].degree; j++) {
          int n = FS_MESH[cell].neighbors[j];
          assert(g.cells[n] & ((g.cells[n] & FS_MINE) ? FS_FLAG : FS_OPEN));
        }
        matched++;
        degrees |= 1u << FS_MESH[cell].degree;
        g = correct;
        assert(classic_flag(&g, mine) && classic_flag(&g, safe));
        assert(classic_reveal(&g, cell)); // Matching but misplaced flags lose.
        assert(g.status == FS_LOST && classic_valid(&g));
        before = g;
        assert(!classic_chord(&g, cell));
        assert(!memcmp(&g, &before, sizeof g));
        wrong++;
        // Leave just one safe cell covered: a chord can finish the board.
        g = correct;
        for (int n = 0; n < FS_CELLS; n++)
          if (n != safe && !(g.cells[n] & FS_MINE)) g.cells[n] |= FS_OPEN;
        assert(classic_valid(&g));
        assert(classic_reveal(&g, cell));
        assert(g.status == FS_WON && classic_valid(&g));
        won++;
      }
    }
  }
  assert(matched && wrong && won);
  assert(degrees & (1u << 6));
#if FS_CELLS == 42
  assert(degrees & (1u << 5));
#endif
  printf("PASS %u matching, %u incorrect, %u winning chords; degree mask %u\n",
         matched, wrong, won, degrees);
}
