#include "../src/c/game.h"
#include "../src/c/geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void){for(int i=0;i<37;i++){assert(HS_MESH[i].degree>=3&&HS_MESH[i].degree<=6);for(int j=0;j<HS_MESH[i].degree;j++){int n=HS_MESH[i].neighbors[j],found=0;assert(n>=0&&n<37);for(int k=0;k<HS_MESH[n].degree;k++)found|=HS_MESH[n].neighbors[k]==i;assert(found);}}
for(unsigned seed=1;seed<=1000;seed++)for(int first=0;first<37;first++){Game g;game_new(&g,seed);assert(game_valid(&g));assert(game_flag(&g,first));assert(!game_reveal(&g,first));game_flag(&g,first);assert(game_reveal(&g,first));assert(game_clue(&g,first)==0&&game_valid(&g));int mines=0;Game loss=g;for(int i=0;i<37;i++)if(g.cells[i]&HS_MINE){mines++;if(loss.status==HS_ACTIVE){game_reveal(&loss,i);assert(loss.status==HS_LOST&&game_valid(&loss));}}else game_reveal(&g,i);assert(mines==6&&g.status==HS_WON&&game_valid(&g));assert(!game_flag(&g,first));}puts("PASS 37000 boards: adjacency, safe opening, flags, flood, win and loss");}
