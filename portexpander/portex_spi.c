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

#define MCP23_S_17_ID 0x0


/* function prototypes */
static int mcp23s17_probe(struct spi_device *spi);
static int mcp23s17_remove(struct spi_device *spi);
static int mcp23s17_suspend(struct spi_device *spi, pm_message_t state);
static int mcp23s17_resume(struct spi_device *spi);



/* MCP23S08 device protocol information */
static struct spi_driver mcp23s08_driver = {
  /* default linux driver device information */
  .driver = {
    .name  = "mcp23s17",
    .bus   = &spi_bus_type,
    .owner = THIS_MODULE,
  },
  /* callback functions for the mcp23s08 */
  .probe   = mcp23s17_probe,   // called by the spi master, when a spi device was found
  .remove  = mcp23s17_remove,  // replace found with removed
  .suspend = mcp23s17_suspend,
  .resume  = mcp23s17_resume,
};

/*
static int mcp23s17_read(unsigned int reg)
{
  return 0;
}
*/

static int mcp23s17_write(enum MCP23S17_ADDR addr, enum MCP23S17_REGS reg, u8 value)
{
  u8 tx[3];

  tx[0] = 0x40 | addr; // datasheet: controlbyte: 01000 A1 A0 R/W
  tx[1] = reg;
  tx[2] = value;
  return 0;
}

static int mcp23s17_probe(struct spi_device *spi)
{
  dev_dbg(&spi->dev, "probing done\n");
  mcp23s17_write(MCP23S17_ADDR_00, MCP23S17_REG_IODIR, 0x00);
  mcp23s17_write(MCP23S17_ADDR_00, MCP23S17_REG_GPIO, 0xFF);
  return 0;
}

static int mcp23s17_remove(struct spi_device *spi)
{
  dev_dbg(&spi->dev, "remove\n");
  return 0;
}

static int mcp23s17_suspend(struct spi_device *spi, pm_message_t state)
{
  return 0;
}

static int mcp23s17_resume(struct spi_device *spi)
{
  return 0;
}

int portex_spi_init(void)
{
  return spi_register_driver(&mcp23s08_driver);
}

void portex_spi_free(void)
{
  spi_unregister_driver(&mcp23s08_driver);
}
