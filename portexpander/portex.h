#ifndef PORTEX_H
#define PORTEX_H 1

#include <linux/printk.h>

#define DRIVER_NAME "portex"


enum MCP23S_ADDR {
  /* MCP23S17 && MCP23S08 compatible */
  MCP23S08_ADDR_00 = 0x0,
  MCP23S08_ADDR_01 = 0x2,
  MCP23S08_ADDR_02 = 0x4,
  MCP23S08_ADDR_03 = 0x6,

  /* MCP23S17 ONLY */
  MCP23S17_ADDR_00 = 0x8,
  MCP23S17_ADDR_01 = 0xA,
  MCP23S17_ADDR_02 = 0xC,
  MCP23S17_ADDR_03 = 0xE,
};

enum MCP23S_REGS {
  /* MCP23S17 && MCP23S08 compatible */
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
  MCP23S17_REG_IODIR   = 0x10,
  MCP23S17_REG_IPOL    = 0x11,
  MCP23S17_REG_GPINTEN = 0x12,
  MCP23S17_REG_DEFVAL  = 0x13,
  MCP23S17_REG_INTCON  = 0x14,
  MCP23S17_REG_IOCON   = 0x15,
  MCP23S17_REG_GPPU    = 0x16,
  MCP23S17_REG_INTF    = 0x17,
  MCP23S17_REG_INTCAP  = 0x18,
  MCP23S17_REG_GPIO    = 0x19,
  MCP23S17_REG_OLAT    = 0x1A,

  REG_MAX // used for device cache, wasting 5 bytes
};


int portex_sysfs_init(void);

void portex_sysfs_free(void);

int portex_spi_init(void);

void portex_spi_free(void);

void portex_write_cached(enum MCP23S_REGS port, u8 pin_mask, u8 value);

int portex_read_cached(enum MCP23S_REGS port, u8 pin_mask);

#endif
