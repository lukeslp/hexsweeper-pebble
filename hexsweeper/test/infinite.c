// Luke Steuber. Infinite field seams, chording, flood budgets and history
// bounds.
#include "infinite.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static void settle(Infinite *g) {
  int ticks = 0;
  while (g->busy && ticks++ < 10000)
    inf_pump(g, 64);
  assert(!g->busy && inf_valid(g));
}
int main(void) {
  int chords = 0, losses = 0;
  for (uint32_t seed = 1; seed <= 300; seed++) {
    Infinite g;
    inf_new(&g, seed);
    InfCoord first = {(int)seed % 19 - 9, (int)seed % 23 - 11};
    assert(inf_valid(&g));
    assert(inf_reveal(&g, first));
    settle(&g);
    assert(inf_clue(&g, first) == 0 && g.opened >= 7);
    for (int q = -18; q <= 18; q++)
      for (int r = -18; r <= 18; r++) {
        InfCoord p = {first.q + q, first.r + r};
        if (!(inf_state(&g, p) & INF_OPEN) || !inf_clue(&g, p))
          continue;
        InfCoord safe = {0}, mine = {0};
        bool has_safe = false, has_mine = false;
        Infinite trial = g;
        for (int d = 0; d < 6; d++) {
          InfCoord n;
          assert(inf_neighbor(p, d, &n));
          if (inf_mine(&g, n)) {
            mine = n;
            has_mine = true;
            assert(inf_flag(&trial, n));
          } else if (!inf_state(&g, n)) {
            safe = n;
            has_safe = true;
          }
        }
        if (!has_safe || !has_mine)
          continue;
        Infinite bad = trial;
        assert(inf_reveal(&trial, p));
        settle(&trial);
        assert(trial.status == INF_ACTIVE);
        chords++;
        assert(inf_flag(&bad, mine) && inf_flag(&bad, safe));
        assert(inf_reveal(&bad, p));
        assert(bad.status == INF_LOST && inf_valid(&bad));
        losses++;
        q = 19;
        break;
      }
    InfCoord distant = {-1000000, 2000000};
    bool layout = inf_mine(&g, distant);
    assert(inf_flag(&g, distant));
    assert(inf_state(&g, distant) == INF_FLAG);
    assert(inf_mine(&g, distant) == layout);
    assert(inf_valid(&g));
    Infinite restored;
    memcpy(&restored, &g, sizeof g);
    assert(inf_valid(&restored));
    assert(inf_state(&restored, distant) == INF_FLAG &&
           inf_mine(&restored, distant) == layout);
  }
  assert(chords > 250 && losses == chords);
  Infinite g;
  inf_new(&g, 91);
  for (int i = 0; i < INF_CHUNKS; i++)
    assert(inf_flag(&g, (InfCoord){i * 80, -i * 80}));
  Infinite before = g;
  assert(!inf_flag(&g, (InfCoord){100000, 0}) && g.full);
  for (int i = 0; i < INF_CHUNKS; i++)
    assert(inf_state(&g, (InfCoord){i * 80, -i * 80}) == INF_FLAG);
  assert(g.flagged == before.flagged && inf_valid(&g));
  assert(inf_flag(
      &g, (InfCoord){0, 0})); // Releasing an empty flagged chunk frees space.
  assert(inf_flag(&g, (InfCoord){100000, 0}) && !g.full);
  assert(inf_valid(&g));
  InfCoord n;
  assert(!inf_neighbor((InfCoord){INT32_MAX, 0}, 0, &n));
  assert(!inf_neighbor((InfCoord){INT32_MIN, 0}, 3, &n));
  inf_new(&g, 1);
  assert(inf_reveal(&g, (InfCoord){INT32_MIN, INT32_MAX}));
  settle(&g);
  printf("PASS %d chord/loss pairs, signed seams, distant flags, storage "
         "exhaustion and coordinate bounds; snapshot %zu bytes\n",
         chords, sizeof g);
}
