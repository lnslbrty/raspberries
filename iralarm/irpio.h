#ifndef IRPIO_H
#define IRPIO_H 1

#include "config.h"

#ifndef USE_WIPI

#define LOW              0
#define HIGH             1

#define INPUT            0
#define OUTPUT           1
#define PWM_OUTPUT       2

#define INT_EDGE_FALLING 1

#define PWM_MODE_MS      0
#define PWM_MODE_BAL     1
#endif /* !USE_WIPI */

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

inline void
irpio_pwmSetMode(int mode);

inline void
irpio_pwmSetClock(int divisor);

inline void
irpio_pwmSetRange(unsigned int range);

inline void
irpio_digitalWrite(int pin, int value);

inline void
irpio_pwmWrite(int pin, int value);

inline int
irpio_isr(int pin, int mode, void (*function)(void));
#endif
