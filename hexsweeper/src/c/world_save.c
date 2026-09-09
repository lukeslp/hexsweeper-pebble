// Luke Steuber. Data bank first, checksummed metadata next, selector last.
#include "world_save.h"
static uint32_t hash(const void *v, size_t n) {
  const uint8_t *p = v;
  uint32_t s = 2166136261u;
  while (n--)
    s = (s ^ *p++) * 16777619u;
  return s;
}
static int selected(const InfIO *io) {
  int32_t bank = 0;
  if (io->read(30, &bank, sizeof bank) != sizeof bank || bank < 0 || bank > 1)
    return 0;
  return bank;
}
static bool bank_load(const InfIO *io, int bank, Infinite *g) {
  uint32_t base = 100 + 32 * bank, meta[3];
  if (io->read(base + 31, meta, sizeof meta) != sizeof meta ||
      meta[0] != 0x48534931 || meta[1] != sizeof *g)
    return false;
  uint8_t *p = (uint8_t *)g;
  for (unsigned o = 0; o < sizeof *g; o += 240) {
    unsigned n = sizeof *g - o;
    if (n > 240)
      n = 240;
    if (io->read(base + o / 240, p + o, n) != (int)n)
      return false;
  }
  return hash(g, sizeof *g) == meta[2] && inf_valid(g);
}
bool inf_load(const InfIO *io, Infinite *g) {
  int bank = selected(io);
  return bank_load(io, bank, g) || bank_load(io, bank ^ 1, g);
}
bool inf_store(const InfIO *io, const Infinite *g) {
  if (!inf_valid(g))
    return false;
  int bank = selected(io) ^ 1;
  Infinite previous;
  if (!bank_load(io, bank ^ 1, &previous) && bank_load(io, bank, &previous)) {
    int32_t fallback = bank;
    if (io->write(30, &fallback, sizeof fallback) != sizeof fallback)
      return false;
    bank ^= 1;
  }
  uint32_t base = 100 + 32 * bank;
  const uint8_t *p = (const uint8_t *)g;
  for (unsigned o = 0; o < sizeof *g; o += 240) {
    unsigned n = sizeof *g - o;
    if (n > 240)
      n = 240;
    if (io->write(base + o / 240, p + o, n) != (int)n)
      return false;
  }
  uint32_t meta[] = {0x48534931, sizeof *g, hash(g, sizeof *g)};
  if (io->write(base + 31, meta, sizeof meta) != sizeof meta)
    return false;
  int32_t selector = bank;
  return io->write(30, &selector, sizeof selector) == sizeof selector;
}
