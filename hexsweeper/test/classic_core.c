#include "../src/c/classic.h"
#include "../src/c/classic_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void){for(int i=0;i<37;i++){assert(FS_MESH[i].degree>=3&&FS_MESH[i].degree<=6);for(int j=0;j<FS_MESH[i].degree;j++){int n=FS_MESH[i].neighbors[j],found=0;assert(n>=0&&n<37);for(int k=0;k<FS_MESH[n].degree;k++)found|=FS_MESH[n].neighbors[k]==i;assert(found);}}
for(unsigned seed=1;seed<=1000;seed++)for(int first=0;first<37;first++){ClassicGame g;classic_new(&g,seed);assert(classic_valid(&g));assert(classic_flag(&g,first));assert(!classic_reveal(&g,first));classic_flag(&g,first);assert(classic_reveal(&g,first));assert(classic_clue(&g,first)==0&&classic_valid(&g));int mines=0;ClassicGame loss=g;for(int i=0;i<37;i++)if(g.cells[i]&FS_MINE){mines++;if(loss.status==FS_ACTIVE){classic_reveal(&loss,i);assert(loss.status==FS_LOST&&classic_valid(&loss));}}else classic_reveal(&g,i);assert(mines==6&&g.status==FS_WON&&classic_valid(&g));assert(!classic_flag(&g,first));}puts("PASS 37000 boards: adjacency, safe opening, flags, flood, win and loss");}
