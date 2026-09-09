// Luke Steuber. Interrupted infinite-world snapshots retain old discoveries.
#include "world_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t records[200][256],backup[200][256];
static int sizes[200],old_sizes[200],writes,fail=-1;
static int rd(uint32_t key,void *p,size_t n){assert(key<200&&n<=256);if(sizes[key]!=(int)n)return -1;memcpy(p,records[key],n);return n;}
static int wr(uint32_t key,const void *p,size_t n){assert(key<200&&n<=256);if(writes++==fail){if(key!=30){memcpy(records[key],p,n/2);sizes[key]=n/2;}return -1;}memcpy(records[key],p,n);sizes[key]=n;return n;}
static const InfIO io={rd,wr};
static void snapshot(void){memcpy(backup,records,sizeof records);memcpy(old_sizes,sizes,sizeof sizes);}
static void reset(void){memcpy(records,backup,sizeof records);memcpy(sizes,old_sizes,sizeof sizes);writes=0;}
int main(void){
 Infinite first,next,loaded;inf_new(&first,42);assert(inf_reveal(&first,(InfCoord){-1,-1}));
 assert(inf_store(&io,&first));assert(inf_load(&io,&loaded)&&!memcmp(&loaded,&first,sizeof first));
 next=first;while(next.busy)inf_pump(&next,64);
 next.focus=(InfCoord){20000,-20000};assert(inf_flag(&next,next.focus));
 snapshot();writes=0;assert(inf_store(&io,&next));int total=writes;
 for(int fault=0;fault<total;fault++){
  reset();fail=fault;assert(!inf_store(&io,&next));
  assert(inf_load(&io,&loaded)&&!memcmp(&loaded,&first,sizeof first));
  fail=-1;assert(inf_store(&io,&next));assert(inf_load(&io,&loaded)&&!memcmp(&loaded,&next,sizeof next));
 }
 int32_t bank;rd(30,&bank,sizeof bank);records[100+32*bank][0]^=1;
 assert(inf_load(&io,&loaded)&&!memcmp(&loaded,&first,sizeof first));snapshot();
 for(int fault=0;fault<total+1;fault++){
  reset();fail=fault;assert(!inf_store(&io,&next));
  assert(inf_load(&io,&loaded)&&!memcmp(&loaded,&first,sizeof first));
 }
 fail=-1;assert(inf_store(&io,&next));
 int bytes=0;for(int i=0;i<200;i++)bytes+=sizes[i];
 assert(bytes+256<4096);
 printf("PASS interrupted flood/flag/focus snapshots and fallback recovery; %d stored bytes plus other-mode reserve\n",bytes);
}
