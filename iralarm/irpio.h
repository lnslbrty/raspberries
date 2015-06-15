#ifndef IRPIO_H
#define IRPIO_H 1

#include "config.h"

#define PIN(pinout_enum) irpio_getpin(pinout_enum)

enum pinout {
  RPIO_PINPOS,
  RPIO_MAX
};


void
irpio_init(void);

void
irpio_lowall(void);

void
irpio_setpwm(int clock, int range);

void
irpio_setvalue(enum pinout p, int value);

void
irpio_selftest(void);

inline int
irpio_getpin(enum pinout p);

#endif
