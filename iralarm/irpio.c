#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiringPi.h>

#include "log.h"
#include "irpio.h"


struct irpio {
  const char *name;
  int pin;
  int mode;
  int enabled;
};

static const struct irpio pipi[] = { RPIO_PINOUT };
static int pwm_range = 1024;


void
irpio_init(void)
{
  int i;

  pwmSetMode(PWM_MODE_BAL);
  for (i = 0; i < RPIO_MAX; i++) {
    if (pipi[i].enabled == 1) {
      pinMode(pipi[i].pin, pipi[i].mode);
      irpio_setvalue(i, 0);
    }
  }
}

void
irpio_lowall(void)
{
  int i;

  for (i = 0; i < RPIO_MAX; i++) {
    if (pipi[i].enabled == 1) {
      irpio_setvalue(i, 0);
    }
  }
}

void
irpio_setpwm(int clock, int range)
{
  pwm_range = range;
  pwmSetClock(clock);
  pwmSetRange(range);
}

void
irpio_setvalue(enum pinout p, int value)
{
  if ( pipi[p].enabled == 1 ) {
    switch (pipi[p].mode) {
      case OUTPUT:
        digitalWrite(pipi[p].pin, value);
        break;
      case PWM_OUTPUT:
        pwmWrite(pipi[p].pin, value);
        break;
      case INPUT:
        break;
    }
  }
}

void
irpio_selftest(void)
{
  int i;

  for (i = 0; i < RPIO_MAX; i++) {
    if (pipi[i].enabled == 1) {
      log("selftesting pin %02d, %s", pipi[i].pin, pipi[i].name);
      irpio_setvalue(i, (pipi[i].mode == PWM_OUTPUT ? pwm_range-1 : 1));
      usleep(10000);
      irpio_setvalue(i, 0);
    }
  }
}

inline int
irpio_getpin(enum pinout p)
{
  return pipi[p].pin;
}

