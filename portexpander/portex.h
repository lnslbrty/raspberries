#ifndef PORTEX_H
#define PORTEX_H 1

#include <linux/printk.h>


#define PRINTP "portex: "
static const char prefix[] = PRINTP;
#define INFO(fmt, ...) printk(KERN_INFO "%s" fmt "\n", prefix, __VA_ARGS__)

enum MCP23S_ADDR {
  MCP23S_ADDR_00 = 0x0,
  MCP23S_ADDR_01 = 0x2,
  MCP23S_ADDR_02 = 0x4,
  MCP23S_ADDR_03 = 0x6,
};

enum MCP23S_REGS {
  /* MCP23S08 compatible */
  MCP23S08_REG_IODIR   = 0x00,
  MCP23S08_REG_IPOL    = 0x01,
  MCP23S08_REG_GPINTEN = 0x02,
  MCP23S08_REG_DEFVAL  = 0x03,
  MCP23S08_REG_INTCON  = 0x04,
  MCP23S08_REG_IOCON   = 0x05,
  MCP23S08_REG_GPPU    = 0x06,
  MCP23S08_REG_INTF    = 0x07,
  MCP23S08_REG_INTCAP  = 0x08,
  MCP23S08_REG_GPIO    = 0x09,
  MCP23S08_REG_OLAT    = 0x0A,

  /* MCP23S17 ONLY */
};


int portex_sysfs_init(void);

void portex_sysfs_free(void);

int portex_spi_init(void);

void portex_spi_free(void);

#endif
