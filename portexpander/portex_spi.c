/**
 * @file   portex_spi.c
 * @author Toni Uhlig, Eric Mueller
 * @date   14.12.2015
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>

#include "portex.h"

#define DRIVER_NAME "mcp23s17"


static const char *drv_name = DRIVER_NAME;
static struct spi_device *spi_mcp23s17 = NULL;

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

  tx[0] = 0x41 | addr; // datasheet: controlbyte: 01000 A1 A0 1
  tx[1] = reg;
  ret = spi_write_then_read(spi_mcp23s17, &tx[0], sizeof(tx), &rx[0], sizeof(rx));
  return (ret < 0 ? ret : rx[0] | rx[1] << 8);
}

static int mcp23s17_write(enum MCP23S_ADDR addr, enum MCP23S_REGS reg, u8 value)
{
  u8 tx[3];

  tx[0] = 0x40 | addr; // datasheet: controlbyte: 01000 A1 A0 0
  tx[1] = reg;
  tx[2] = value;
  return spi_write(spi_mcp23s17, &tx[0], sizeof(tx));
}

static int mcp23s17_probe(struct spi_device *spi)
{
  pr_info("%s: %s()\n", drv_name, __func__);

  if (mcp23s17_read(MCP23S_ADDR_00, MCP23S08_REG_IODIR) != 0xFFFF) {
    return -ENODEV;
  }

  mcp23s17_write(MCP23S_ADDR_00, MCP23S08_REG_IODIR, 0x00);
  mcp23s17_write(MCP23S_ADDR_00, MCP23S08_REG_GPIO,  0xFF);

  return 0;
}

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
    pr_err("%s: Deleting %s\n", drv_name, str);
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

  /* delete all registered devices (registered by bcm2708_spi) */
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
  mcp23s17_write(MCP23S_ADDR_00, MCP23S08_REG_GPIO,  0x00);
  mcp23s17_write(MCP23S_ADDR_00, MCP23S08_REG_IODIR, 0xFF);

  spi_unregister_driver(&mcp23s17_driver);
  if (spi_mcp23s17) {
    device_del(&spi_mcp23s17->dev);
    kfree(spi_mcp23s17);
  }
}
