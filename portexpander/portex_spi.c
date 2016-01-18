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

#define MCP23_S_8_ID 0x0


/* function prototypes */
static int mcp23s08_probe(struct spi_device *spi);
static int mcp23s08_remove(struct spi_device *spi);


/* device structure for the kernel device table */
static const struct spi_device_id mcp23s08_ids[] = {
  { "mcp23s08", MCP23_S_8_ID },
  { },
};
MODULE_DEVICE_TABLE(spi, mcp23s08_ids);

/* device match structure */
/* (detect the device if plugged in) */
static const struct of_device_id mcp23s08_spi_of_match[] = {
  {
    .compatible = "microchip,mcp23s08",
    .data = (void *) MCP23_S_8_ID,
  },
};

/* MCP23S08 device protocol information */
static struct spi_driver mcp23s08_driver = {
  .id_table =  mcp23s08_ids,
  /* default linux driver device information */
  .driver = {
    .name           = "mcp23s08",
    .owner          = THIS_MODULE,
    .of_match_table = of_match_ptr(mcp23s08_spi_match),
  },
  /* callback functions for the mcp23s08 */
  .probe  = mcp23s08_probe, // called by the spi master, when a spi device was found
  .remove = mcp23s08_remove // replace found with removed
};

/*
static int mcp23s08_read(unsigned int reg)
{
  return 0;
}
*/

static int mcp23s08_write(enum MCP23S08_ADDR addr, enum MCP23S08_REGS reg, u8 value)
{
  u8 tx[3];

  tx[0] = 0x40 | addr; // datasheet: controlbyte: 01000 A1 A0 R/W
  tx[1] = reg;
  tx[2] = value;
  return 0;
}

static int mcp23s08_probe(struct spi_device *spi)
{
  const struct of_device_id *match = of_match_device(of_match_ptr(mcp23s08_spi_of_match), &spi->dev);
  int status, type;
  u32 spi_present_mask = 0;

  if (match) {
    type   = (int)(uintptr_t)match->data;
    /* check if mcp23s08 device mask available */
    status = of_property_read_u32(spi->dev.of_node, "microchip,spi-present-mask", &spi_present_mask);
    if (status) {
      dev_err(&spi->dev, "DeviceTree has no spi-present-mask\n");
      return -ENODEV;
    }
    /* check if mcp23s08 device mask valid */
    if ((spi_present_mask <= 0) || (spi_present_mask >= 256)) {
      dev_err(&spi->dev, "invalid spi-present-mask\n");
      return -ENODEV;
    }
  }

  dev_dbg(&spi->dev, "probing done\n");
  mcp23s08_write(MCP23S08_ADDR_00, MCP23S08_REG_GPIO, 0xFF);
  return 0;
}

static int mcp23s08_remove(struct spi_device *spi)
{
  dev_dbg(&spi->dev, "remove\n");
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
