// Luke Steuber. Read-only host planner for native gesture/chord verification.
#include "game.h"
#include "geometry.h"
#include "classic.h"
#include "classic_geometry.h"
#include "infinite.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 int mode=atoi(argv[1]);unsigned seed=strtoul(argv[2],NULL,10);
 Game g;ClassicGame f;Infinite w;
 if(mode==0){classic_new(&f,seed);classic_reveal(&f,18);}
 if(mode==2){game_new(&g,seed);game_reveal(&g,0);}
 if(mode==1){inf_new(&w,seed);inf_reveal(&w,(InfCoord){0,0});while(w.busy)inf_pump(&w,64);}
 printf("{\"rng\":%u,\"seed\":%u,\"open\":%d,\"cells\":[",mode==0?f.seed:mode==2?g.seed:w.seed,seed,mode==0?classic_open_count(&f):mode==2?game_open_count(&g):(int)w.opened);
 int first=1;
 int lo=mode==1?-12:0, hi=mode==1?12:mode==0?36:41;
 for(int a=lo;a<=hi;a++)for(int b=mode==1?-12:0;b<=(mode==1?12:0);b++){
  int open=mode==0?!!(f.cells[a]&FS_OPEN):mode==2?!!(g.cells[a]&HS_OPEN):!!(inf_state(&w,(InfCoord){a,b})&INF_OPEN);
  int clue=mode==0?classic_clue(&f,a):mode==2?game_clue(&g,a):inf_clue(&w,(InfCoord){a,b});
  int q=mode==0?FS_XY[a][0]:a,r=mode==0?FS_XY[a][1]:b;
  printf("%s[%d,%d,%d,%d,%d,%d]",first?"":",",a,b,q,r,open,clue);first=0;
 }
 printf("],\"plans\":[");first=1;
 for(int a=lo;a<=hi;a++)for(int b=mode==1?-12:0;b<=(mode==1?12:0);b++){
  int open=mode==0?!!(f.cells[a]&FS_OPEN):mode==2?!!(g.cells[a]&HS_OPEN):!!(inf_state(&w,(InfCoord){a,b})&INF_OPEN);
  int clue=mode==0?classic_clue(&f,a):mode==2?game_clue(&g,a):inf_clue(&w,(InfCoord){a,b});
  if(!open||!clue)continue;
  int nq[6],nr[6],mines[6],safe=-1,count=0,n=mode==0?FS_MESH[a].degree:mode==2?HS_MESH[a].degree:6;
  for(int d=0;d<n;d++){
   InfCoord p={a,b};if(mode==1)inf_neighbor(p,d,&p);
   nq[d]=mode==0?FS_MESH[a].neighbors[d]:mode==2?HS_MESH[a].neighbors[d]:p.q;nr[d]=mode==1?p.r:0;
   mines[d]=mode==0?!!(f.cells[nq[d]]&FS_MINE):mode==2?!!(g.cells[nq[d]]&HS_MINE):inf_mine(&w,p);
   int covered=mode==0?!(f.cells[nq[d]]&FS_OPEN):mode==2?!(g.cells[nq[d]]&HS_OPEN):!(inf_state(&w,p)&INF_OPEN);
   if(mines[d])count++;else if(covered)safe=d;
  }
  if(!count||safe<0)continue;
  printf("%s{\"target\":[%d,%d],\"safe\":[%d,%d],\"mines\":[",first?"":",",a,b,nq[safe],nr[safe]);first=0;
  int printed=0;for(int d=0;d<n;d++)if(mines[d])printf("%s[%d,%d]",printed++?",":"",nq[d],nr[d]);printf("]}");
 }
 puts("]}");return 0;
}
