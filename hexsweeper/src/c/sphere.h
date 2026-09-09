#ifndef HS_SPHERE_H
#define HS_SPHERE_H
#include "game.h"
typedef struct {
  float x, y, z;
} Vec;
typedef struct {
  float w, x, y, z;
} Quat;
typedef struct {
  float x, y;
} Point;
typedef struct {
  int count;
  Point p[8];
  Vec center;
} Polygon;
typedef struct {
  float x, y, r;
} Viewport;
Quat sphere_identity(void);
Quat sphere_multiply(Quat a, Quat b);
Quat sphere_between(Vec a, Vec b);
Vec sphere_rotate(Quat q, Vec p);
Vec sphere_center(int cell);
Quat sphere_center_on(Quat q, int cell);
Vec sphere_arcball(Viewport v, float x, float y);
void sphere_polygon(Quat q, Viewport v, int cell, Polygon *out);
int sphere_pick(Quat q, Viewport v, float x, float y);
int sphere_front(Quat q);
int sphere_neighbors(Quat q, int cell, uint8_t out[6]);
bool sphere_quat_valid(Quat q);
#endif
