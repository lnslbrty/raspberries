/**
 * @file   portex_spi.c
 * @author Toni Uhlig, Eric Mueller
 * @date   14.12.2015
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include "portex.h"


static const char *drv_name = DRIVER_NAME;

/* TODO: support more than one device */

/* per device declarations */
static struct spi_device *spi_mcp23s17 = NULL;
static u8 cache_mcp23s17[REG_MAX]; // per device register cache

/* function prototypes */
static int mcp23s17_probe(struct spi_device *spi);
static int mcp23s17_remove(struct spi_device *spi);


/* MCP23S17 slave info */
static struct spi_board_info mcp23s17_info = {
  .modalias = DRIVER_NAME,
  .max_speed_hz = 10000000,
  .bus_num = 0,
  .chip_select = 0,
  .mode = SPI_MODE_0,
};

/* MCP23S17 device driver information */
static struct spi_driver mcp23s17_driver = {
  /* default linux driver device information */
  .driver = {
    .name  = DRIVER_NAME,
    .owner = THIS_MODULE,
  },
  /* callback functions for the mcp23s08 */
  .probe   = mcp23s17_probe,   // called by the spi master, when a spi device was found
  .remove  = mcp23s17_remove,  // replace found with removed
};

static int mcp23s17_read(enum MCP23S_ADDR addr, enum MCP23S_REGS reg)
{
  int ret;
  u8 tx[2], rx[2];

  tx[0] = 0x41 | addr; // datasheet: controlbyte: 0100 (A2) A1 A0 1
  tx[1] = reg;
  ret = spi_write_then_read(spi_mcp23s17, &tx[0], sizeof(tx), &rx[0], sizeof(rx));
  return (ret < 0 ? ret : rx[1] | rx[0] << 8);
}

static int mcp23s17_write(enum MCP23S_ADDR addr, enum MCP23S_REGS reg, u8 value)
{
  u8 tx[3];

  tx[0] = 0x40 | addr; // datasheet: controlbyte: 0100 (A2) A1 A0 0
  tx[1] = reg;
  tx[2] = value;
  return spi_write(spi_mcp23s17, &tx[0], sizeof(tx));
}

/* TODO: maybe use async io? */
void portex_write_cached(enum MCP23S_REGS reg, u8 pin_mask, u8 value)
{
  if (!value) {
    cache_mcp23s17[reg] &= ~pin_mask;
  } else {
    cache_mcp23s17[reg] |= pin_mask;
  }
  mcp23s17_write(MCP23S08_ADDR_00, reg, cache_mcp23s17[reg]);
}

/* TODO: Interrupt if pin direction is INPUT */
int portex_read_cached(enum MCP23S_REGS reg, u8 pin_nmb)
{
  //cache_mcp23s17[port] = (mcp23s17_read(MCP23S08_ADDR_00, port) & 0xFF);
  return (cache_mcp23s17[reg] >> pin_nmb) & 0x1;
}

static int mcp23s17_probe(struct spi_device *spi)
{
  u16 probe_retval;

  pr_info("%s: %s()\n", drv_name, __func__);

  if ((probe_retval = mcp23s17_read(MCP23S08_ADDR_00, MCP23S08_REG_IODIR)) != 0xFFFF) {
    pr_err("%s: %s() mcp23s17_read(iodir: %d) returned %u\n", drv_name, __func__, MCP23S08_REG_IODIR, probe_retval);
    return -ENODEV;
  }
  if ((probe_retval = mcp23s17_read(MCP23S08_ADDR_00, MCP23S17_REG_IODIR)) != 0xFFFF) {
    pr_err("%s: %s() mcp23s17_read(iodir: %d) returned %u\n", drv_name, __func__, MCP23S17_REG_IODIR, probe_retval);
    return -ENODEV;
  }
  /* set all pins direction to out (TODO: sysfs should set the direction) */
  /* TODO: initially read all registers? */
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S08_REG_IODIR, 0x00);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S17_REG_IODIR, 0x00);
/*
  // debug stuff
  portex_write_cached(MCP23S08_REG_GPIO, 0x01, 1);
  portex_write_cached(MCP23S17_REG_GPIO, 0x01, 1);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S08_REG_GPIO,  0xFF);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S17_REG_GPIO,  0xFF);
*/
  pr_info("%s: %s() successfully finished\n", drv_name, __func__);
  return 0;
}

/* TODO: on remove, something should happen here */
static int mcp23s17_remove(struct spi_device *spi)
{
  dev_dbg(&spi->dev, "%s()\n", __func__);
  return 0;
}

static void spidevices_delete(struct spi_master *master, unsigned cs)
{
  struct device *dev;
  char str[32];

  snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);

  dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
  if (dev) {
    pr_warn("%s: deleting %s\n", drv_name, str);
    device_del(dev);
  }
}

int portex_spi_init(void)
{
  struct spi_master *master;

  pr_info("%s: %s()\n", drv_name, __func__);

  master = spi_busnum_to_master(mcp23s17_info.bus_num);
  if (!master) {
    pr_err("%s: spi_busnum_to_master(%d) returned NULL\n", drv_name, mcp23s17_info.bus_num);
    return -EINVAL;
  }

  /* delete all registered devices using the same cs (registered by bcm2708_spi) */
  spidevices_delete(master, mcp23s17_info.chip_select);

  spi_mcp23s17 = spi_new_device(master, &mcp23s17_info);
  put_device(&master->dev);
  if (!spi_mcp23s17) {
     pr_err("%s: spi_new_device() returned NULL\n", drv_name);
     return -EPERM;
  }

  return spi_register_driver(&mcp23s17_driver);
}

void portex_spi_free(void)
{
  pr_info("%s: %s()\n", drv_name, __func__);

  /* restore default IODIR/GPIO values */
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S08_REG_GPIO,  0x00);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S08_REG_IODIR, 0xFF);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S17_REG_GPIO,  0x00);
  mcp23s17_write(MCP23S08_ADDR_00, MCP23S17_REG_IODIR, 0xFF);

  spi_unregister_driver(&mcp23s17_driver);
  if (spi_mcp23s17) {
    device_del(&spi_mcp23s17->dev);
    kfree(spi_mcp23s17);
  }
}
