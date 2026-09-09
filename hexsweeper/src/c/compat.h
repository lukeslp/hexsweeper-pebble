#ifndef HS_COMPAT_H
#define HS_COMPAT_H
#include <stdbool.h>
#include <stdint.h>

// 4.33 changed TouchEvent's layout; 4.33.2 is the tested touch baseline.
// Never subscribe using the new event layout on an older firmware.
static inline bool firmware_allows_touch(uint8_t major, uint8_t minor,
                                         uint8_t patch) {
  return major > 4 ||
         (major == 4 && (minor > 33 || (minor == 33 && patch >= 2)));
}
#endif
