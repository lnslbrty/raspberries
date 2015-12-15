#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <ctype.h>
#include <pthread.h>
#include <poll.h>
#include <stropts.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef USE_WIPI
#include <wiringPi.h>
#endif

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

#ifndef USE_WIPI
#define BLOCK_SIZE   (4*1024)
#define PI_BCM2708_PBASE  0x20000000
#define PI2_BCM2708_PBASE 0x3F000000
static volatile unsigned int BCM2708_PERI_BASE = PI_BCM2708_PBASE;
#define CLOCK_BASE   (BCM2708_PERI_BASE + 0x00101000)
#define GPIO_BASE    (BCM2708_PERI_BASE + 0x00200000)
#define GPIO_PWM     (BCM2708_PERI_BASE + 0x0020C000)
#define PI_GPIO_MASK (0xFFFFFFC0)
#define BCM_PASSWORD 0x5A000000

#define PWM_CONTROL  0
#define PWMCLK_CNTL  40
#define PWMCLK_DIV   41
#define PWM0_ENABLE  0x0001
#define PWM1_ENABLE  0x0100
#define PWM0_MS_MODE 0x0080
#define PWM1_MS_MODE 0x8000
#define PWM0_RANGE   4
#define PWM1_RANGE   8
#define PWM0_DATA    5
#define PWM1_DATA    9

#define FSEL_ALT0    0b100
#define FSEL_ALT5    0b010

static volatile uint32_t *gpio;
static volatile uint32_t *pwm;
static volatile uint32_t *clk;
static volatile const uint8_t *pinToGpio;
static uint8_t piModel2 = 0;

// sysFds:
//      Map a file descriptor from the /sys/class/gpio/gpioX/value
static int sysFds [64] =
{
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// ISR Data
static void (*isrFunctions[64])(void);
static pthread_mutex_t pinMutex;
static volatile int pinPass = -1;

// Revision 1, 1.1:
static const int pinToGpioR1[64] =
{
  17, 18, 21, 22, 23, 24, 25, 4,        // From the Original Wiki - GPIO 0 through 7:   wpi  0 -  7
   0,  1,                               // I2C  - SDA1, SCL1                            wpi  8 -  9
   8,  7,                               // SPI  - CE1, CE0                              wpi 10 - 11
  10,  9, 11,                           // SPI  - MOSI, MISO, SCLK                      wpi 12 - 14
  14, 15,                               // UART - Tx, Rx                                wpi 15 - 16
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,       // ... 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // ... 63
};

// Revision 2:
static const int pinToGpioR2[64] =
{
  17, 18, 27, 22, 23, 24, 25, 4,        // From the Original Wiki - GPIO 0 through 7:   wpi  0 -  7
   2,  3,                               // I2C  - SDA0, SCL0                            wpi  8 -  9
   8,  7,                               // SPI  - CE1, CE0                              wpi 10 - 11
  10,  9, 11,                           // SPI  - MOSI, MISO, SCLK                      wpi 12 - 14
  14, 15,                               // UART - Tx, Rx                                wpi 15 - 16
  28, 29, 30, 31,                       // Rev 2: New GPIOs 8 though 11                 wpi 17 - 20
   5,  6, 13, 19, 26,                   // B+                                           wpi 21, 22, 23, 24, 25
  12, 16, 20, 21,                       // B+                                           wpi 26, 27, 28, 29
   0,  1,                               // B+                                           wpi 30, 31
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // ... 47
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // ... 63
};

// gpioToGPFSEL:
//      Map a BCM_GPIO pin to it's Function Selection
//      control port. (GPFSEL 0-5)
//      Groups of 10 - 3 bits per Function - 30 bits per port
static const uint8_t gpioToGPFSEL[] =
{
  0,0,0,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,
};

// gpioToShift
//      Define the shift up for the 3 bits per pin in each GPFSEL port
static const uint8_t gpioToShift[] =
{
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
  0,3,6,9,12,15,18,21,24,27,
};

// gpioToPwmALT
//      the ALT value to put a GPIO pin into PWM mode
static const uint8_t gpioToPwmALT[] =
{
          0,         0,         0,         0,         0,         0,         0,         0,       //  0 ->  7
          0,         0,         0,         0, FSEL_ALT0, FSEL_ALT0,         0,         0,       //  8 -> 15
          0,         0, FSEL_ALT5, FSEL_ALT5,         0,         0,         0,         0,       // 16 -> 23
          0,         0,         0,         0,         0,         0,         0,         0,       // 24 -> 31
          0,         0,         0,         0,         0,         0,         0,         0,       // 32 -> 39
  FSEL_ALT0, FSEL_ALT0,         0,         0,         0, FSEL_ALT0,         0,         0,       // 40 -> 47
          0,         0,         0,         0,         0,         0,         0,         0,       // 48 -> 55
          0,         0,         0,         0,         0,         0,         0,         0,       // 56 -> 63
};

static const uint8_t gpioToPwmPort[] =
{
          0,         0,         0,         0,         0,         0,         0,         0,       //  0 ->  7
          0,         0,         0,         0, PWM0_DATA, PWM1_DATA,         0,         0,       //  8 -> 15
          0,         0, PWM0_DATA, PWM1_DATA,         0,         0,         0,         0,       // 16 -> 23
          0,         0,         0,         0,         0,         0,         0,         0,       // 24 -> 31
          0,         0,         0,         0,         0,         0,         0,         0,       // 32 -> 39
  PWM0_DATA, PWM1_DATA,         0,         0,         0, PWM1_DATA,         0,         0,       // 40 -> 47
          0,         0,         0,         0,         0,         0,         0,         0,       // 48 -> 55
          0,         0,         0,         0,         0,         0,         0,         0,       // 56 -> 63

};

// gpioToGPSET:
//      (Word) offset to the GPIO Set registers for each GPIO pin
static const uint8_t gpioToGPSET[] =
{
   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
   8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

// gpioToGPCLR:
//      (Word) offset to the GPIO Clear registers for each GPIO pin
static const uint8_t gpioToGPCLR[] =
{
  10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
  11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
};


static int
piBoardRev (void)
{
  FILE *cpuFd;
  char line[120];
  char *c;
  static int boardRev = -1;

  if (boardRev != -1)   // No point checking twice
    return boardRev;
  if ((cpuFd = fopen("/proc/cpuinfo", "r")) == NULL)
    return -1;

  // Start by looking for the Architecture, then we can look for a B2 revision....
  while (fgets(line, 120, cpuFd) != NULL)
    if (strncmp(line, "Hardware", 8) == 0)
      break;

  if (strncmp(line, "Hardware", 8) != 0)
    return -2;

  // See if it's BCM2708 or BCM2709
  if (strstr(line, "BCM2709") != NULL)
    piModel2 = 1;
  else if (strstr(line, "BCM2708") == NULL)
    return -3;
  
  // Now do the rest of it as before
  rewind(cpuFd);
  while (fgets(line, 120, cpuFd) != NULL)
    if (strncmp(line, "Revision", 8) == 0)
      break;
  fclose(cpuFd) ;
  
  if (strncmp(line, "Revision", 8) != 0)
    return -4;
  
  // Chomp trailing CR/NL
  for (c = &line[strlen(line) - 1]; (*c == '\n') || (*c == '\r'); --c)
    *c = 0;
  
  // Scan to first digit
  for (c = line ; *c ; ++c)
    if (isdigit(*c))
      break;
  
  if (!isdigit(*c))
    return -5;
  
  // Make sure its long enough
  if (strlen(c) < 4)
    return -6;
  
  // Isolate  last 4 characters:
  c = c + strlen(c) - 4;
  
  if ( (strcmp(c, "0002") == 0) || (strcmp(c, "0003") == 0))
    boardRev = 1;
  else
    boardRev = 2;      // Covers everything else from the B revision 2 to the B+, the Pi v2 and CM's.
  
  return boardRev;
}

static int
__init_gpio(void)
{
  int fd, boardRev = piBoardRev();

  if (geteuid () != 0) return -1;

  if (boardRev == 1) {
    pinToGpio = (const uint8_t *)pinToGpioR1;
  } else {
    if (piModel2) {
      BCM2708_PERI_BASE = PI2_BCM2708_PBASE;
    }
    pinToGpio = (const uint8_t *)pinToGpioR2;
  }

  if ( (fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0 ) {
    goto error;
  }
  if ( (gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE)) == (uint32_t *)-1 ) {
    goto error;
  }
  if ( (pwm  = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_PWM)) == (uint32_t *)-1 ) {
    goto error;
  }
  if ( (clk  = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CLOCK_BASE)) == (uint32_t *)-1 ) {
    goto error;
  }
  return 0;
error:
  close(fd);
  return errno;
}

static void
pwmSetMode(int mode)
{
  if (mode == PWM_MODE_MS)
    *(pwm + PWM_CONTROL) = PWM0_ENABLE | PWM1_ENABLE | PWM0_MS_MODE | PWM1_MS_MODE;
  else
    *(pwm + PWM_CONTROL) = PWM0_ENABLE | PWM1_ENABLE;
}

static void
pwmSetClock(int divisor)
{
  uint32_t pwm_control ;
  divisor &= 4095 ;

  pwm_control = *(pwm + PWM_CONTROL);                 // preserve PWM_CONTROL

  *(pwm + PWM_CONTROL) = 0;                           // Stop PWM
  *(clk + PWMCLK_CNTL) = BCM_PASSWORD | 0x01;         // Stop PWM Clock
  usleep(110);                                        // prevents clock going sloooow

  while ((*(clk + PWMCLK_CNTL) & 0x80) != 0)          // Wait for clock to be !BUSY
    usleep(1);

  *(clk + PWMCLK_DIV)  = BCM_PASSWORD | (divisor << 12);
  *(clk + PWMCLK_CNTL) = BCM_PASSWORD | 0x11;         // Start PWM clock
  *(pwm + PWM_CONTROL) = pwm_control;                 // restore PWM_CONTROL
}

static void
pwmSetRange(unsigned int range)
{
  *(pwm + PWM0_RANGE) = range;
  usleep(10);
  *(pwm + PWM1_RANGE) = range;
  usleep(10);
}

static void
pinMode(int pin, int mode)
{
  int fSel, shift, alt;

  if ((pin & PI_GPIO_MASK) == 0) { // On-board pin
    pin   = pinToGpio[pin];
    fSel  = gpioToGPFSEL[pin];
    shift = gpioToShift[pin];

    switch (mode) {
      case INPUT:
        *(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) ; // Sets bits to zero = input
        break;
      case OUTPUT:
        *(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) | (1 << shift);
        break;
      case PWM_OUTPUT:
        if ((alt = gpioToPwmALT [pin]) == 0) {      // Not a hardware capable PWM pin
          return;
        }
        *(gpio + fSel) = (*(gpio + fSel) & ~(7 << shift)) | (alt << shift);
        usleep(110);
        pwmSetMode(PWM_MODE_BAL); // Pi default mode
        pwmSetRange(1024);        // Default range of 1024
        pwmSetClock(32);          // 19.2 / 32 = 600KHz - Also starts the PWM
        break;
    }
  }
}

static void
digitalWrite(int pin, int value)
{
  if ((pin & PI_GPIO_MASK) == 0)                // On-Board Pin
  {
    if (value == LOW)
      *(gpio + gpioToGPCLR [pin]) = 1 << (pin & 31) ;
    else
      *(gpio + gpioToGPSET [pin]) = 1 << (pin & 31) ;
  }
}

static void
pwmWrite(int pin, int value)
{
  if ((pin & PI_GPIO_MASK) == 0)                // On-Board Pin
  {
    *(pwm + gpioToPwmPort [pin]) = value ;
  }
}

static int waitForInterrupt (int pin, int mS)
{
  int fd, x;
  uint8_t c;
  struct pollfd polls;

  if ((fd = sysFds[pin]) == -1)
    return -2;

  // Setup poll structure
  polls.fd     = fd ;
  polls.events = POLLPRI;      // Urgent data!

  // Wait for it ...
  x = poll(&polls, 1, mS);

  // Do a dummy read to clear the interrupt
  //      A one character read appars to be enough.
  //      Followed by a seek to reset it.
  (void)read (fd, &c, 1);
  lseek (fd, 0, SEEK_SET);

  return x;
}

static void *
interruptHandler (void *arg)
{
  int myPin;

  myPin   = pinPass;
  pinPass = -1;

  for (;;)
    if (waitForInterrupt (myPin, -1) > 0)
      isrFunctions[myPin] ();

  return NULL;
}

static int
__isr_init(int pin, int mode, void (*function)(void))
{
  int count, i;
  char fName[64], c;
  pthread_t threadId;

  if (sysFds[pin] == -1) {
    sprintf(fName, "/sys/class/gpio/gpio%d/value", pin);
    if ((sysFds[pin] = open(fName, O_RDWR)) < 0)
      return -1;
  }

  // Clear any initial pending interrupt
  ioctl(sysFds[pin], FIONREAD, &count);
  for (i = 0; i < count; ++i)
    read(sysFds[pin], &c, 1);

  isrFunctions[pin] = function;

  pthread_mutex_lock(&pinMutex);
  pinPass = pin;
  pthread_create(&threadId, NULL, interruptHandler, NULL);
  while (pinPass != -1)
    sleep(1);
  pthread_mutex_unlock(&pinMutex);
  return 0;
}
#endif /* !USE_WIPI */

void
irpio_init(void)
{
  int i;

#ifndef USE_WIPI
  if ( __init_gpio() != 0 ) {
    perror("init_gpio");
    return;
  }
#endif
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

inline void
irpio_pwmSetMode(int mode)
{
  pwmSetMode(mode);
}

inline void
irpio_pwmSetClock(int divisor)
{
  pwmSetClock(divisor);
}

inline void
irpio_pwmSetRange(unsigned int range)
{
  pwmSetRange(range);
}

inline void
irpio_digitalWrite(int pin, int value)
{
  digitalWrite(pin, value);
}

inline void
irpio_pwmWrite(int pin, int value)
{
  pwmWrite(pin, value);
}

inline int
irpio_isr(int pin, int mode, void (*function)(void))
{
#ifndef USE_WIPI
  return __isr_init(pin, mode, function);
#else
  return wiringPiISR(pin, mode, function);
#endif
}

