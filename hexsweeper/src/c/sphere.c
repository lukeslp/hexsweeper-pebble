// Luke Steuber. Orthographic projection and hemisphere clipping.
#include "sphere.h"
#include "geometry.h"
#include <math.h>
static Vec add(Vec a, Vec b) { return (Vec){a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec scale(Vec a, float k) { return (Vec){a.x * k, a.y * k, a.z * k}; }
static float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec cross(Vec a, Vec b) {
  return (Vec){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}
static Vec normalize(Vec v) {
  float n = sqrtf(dot(v, v));
  return n > 1e-7f ? scale(v, 1 / n) : (Vec){0, 0, 1};
}
static Vec unpack(MeshPoint p) {
  return (Vec){p.x / 32767.f, p.y / 32767.f, p.z / 32767.f};
}
static Quat qnormal(Quat q) {
  float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  return n > 1e-7f ? (Quat){q.w / n, q.x / n, q.y / n, q.z / n}
                   : sphere_identity();
}
Quat sphere_identity(void) { return (Quat){1, 0, 0, 0}; }
Quat sphere_multiply(Quat a, Quat b) {
  return qnormal((Quat){a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w});
}
Quat sphere_between(Vec a, Vec b) {
  a = normalize(a);
  b = normalize(b);
  float d = dot(a, b);
  Vec axis = cross(a, b);
  if (d < -.9999f) {
    axis =
        normalize(cross(a, fabsf(a.x) < .8f ? (Vec){1, 0, 0} : (Vec){0, 1, 0}));
    return (Quat){0, axis.x, axis.y, axis.z};
  }
  return qnormal((Quat){1 + d, axis.x, axis.y, axis.z});
}
Vec sphere_rotate(Quat q, Vec p) {
  Vec u = {q.x, q.y, q.z};
  Vec t = scale(cross(u, p), 2);
  return add(p, add(scale(t, q.w), cross(u, t)));
}
Vec sphere_center(int cell) { return unpack(HS_MESH[cell].center); }
Quat sphere_center_on(Quat q, int cell) {
  return sphere_multiply(
      sphere_between(sphere_rotate(q, sphere_center(cell)), (Vec){0, 0, 1}), q);
}
Vec sphere_arcball(Viewport v, float x, float y) {
  Vec p = {(x - v.x) / v.r, (v.y - y) / v.r, 0};
  float d = p.x * p.x + p.y * p.y;
  if (d <= 1)
    p.z = sqrtf(1 - d);
  else
    p = normalize(p);
  return p;
}
void sphere_polygon(Quat q, Viewport v, int cell, Polygon *out) {
  const MeshCell *m = &HS_MESH[cell];
  Vec a[6], b[8];
  int n = 0;
  out->center = sphere_rotate(q, unpack(m->center));
  for (int i = 0; i < m->degree; i++)
    a[i] = sphere_rotate(q, unpack(m->boundary[i]));
  for (int i = 0; i < m->degree; i++) {
    Vec p = a[i], r = a[(i + 1) % m->degree];
    if (p.z >= 0)
      b[n++] = p;
    if ((p.z >= 0) != (r.z >= 0)) {
      float t = p.z / (p.z - r.z);
      b[n++] = add(p, scale(add(r, scale(p, -1)), t));
    }
  }
  out->count = n;
  for (int i = 0; i < n; i++)
    out->p[i] = (Point){v.x + v.r * b[i].x, v.y - v.r * b[i].y};
}
static bool inside(const Polygon *p, float x, float y) {
  bool in = false;
  for (int i = 0, j = p->count - 1; i < p->count; j = i++) {
    Point a = p->p[i], b = p->p[j];
    if ((a.y > y) != (b.y > y) &&
        x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
      in = !in;
  }
  return in;
}
int sphere_pick(Quat q, Viewport v, float x, float y) {
  int best = -1;
  float z = -2;
  Polygon p;
  for (int i = 0; i < HS_CELLS; i++) {
    sphere_polygon(q, v, i, &p);
    if (p.count >= 3 && p.center.z > z && inside(&p, x, y)) {
      best = i;
      z = p.center.z;
    }
  }
  return best;
}
int sphere_front(Quat q) {
  float z = -2;
  int best = 0;
  for (int i = 0; i < HS_CELLS; i++) {
    float d = sphere_rotate(q, sphere_center(i)).z;
    if (d > z) {
      z = d;
      best = i;
    }
  }
  return best;
}
int sphere_neighbors(Quat q, int cell, uint8_t out[6]) {
  float angles[6];
  int n = HS_MESH[cell].degree;
  for (int i = 0; i < n; i++) {
    out[i] = HS_MESH[cell].neighbors[i];
    Vec p = sphere_rotate(q, sphere_center(out[i]));
    angles[i] = atan2f(-p.y, p.x);
  }
  for (int i = 1; i < n; i++) {
    float a = angles[i];
    uint8_t t = out[i];
    int j = i;
    while (j > 0 && angles[j - 1] > a) {
      angles[j] = angles[j - 1];
      out[j] = out[j - 1];
      j--;
    }
    angles[j] = a;
    out[j] = t;
  }
  return n;
}
bool sphere_quat_valid(Quat q) {
  float n = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
  return isfinite(n) && n > .98f && n < 1.02f;
}
