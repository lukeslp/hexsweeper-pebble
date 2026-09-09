// Luke Steuber. Atomic snapshots of retained infinite exploration.
#pragma once
#include "infinite.h"
#include <stddef.h>
typedef struct {
  int (*read)(uint32_t, void *, size_t);
  int (*write)(uint32_t, const void *, size_t);
} InfIO;
bool inf_load(const InfIO *, Infinite *);
bool inf_store(const InfIO *, const Infinite *);
