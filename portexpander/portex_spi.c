/**
 * @file   portex_spi.c
 * @author Toni Uhlig, Eric Mueller
 * @date   14.12.2015
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>	// spi functions/macros

#include "portex.h"


/* MCP23S08 device information */
struct spi_board_info spi_device_info = {
  .modalias = "portex", /* module name */
  .max_speed_hz = 1000000, /* MCP23S08 has 1Mhz clock */
  .bus_num = 0, /* unused -> spi_new_device does not need this */
  .chip_select = 0, /* default -> 0 */
  .mode = SPI_CS_HIGH, /* chip_select is high(flank) -> data is read/write */
};
static const uint spi_bits_per_word = 8;
/* spi device (MCP23S08) */
static struct spi_device *spi_device;
/* spi bus master */
static struct spi_master *spi_master;


int portex_spi_init(void)
{
  int ret;

  if ( (spi_master = spi_busnum_to_master( spi_device_info.bus_num )) == 0 ) {
    INFO("spi_busnum_to_master(%d) failed", spi_device_info.bus_num);
    return -ENODEV;
  }
  INFO("bus_master; bus_num: %d; mode: %u; min_speed: %u; max_speed: %u; bits_per_word: %u", spi_master->bus_num, spi_master->mode_bits, spi_master->min_speed_hz, spi_master->max_speed_hz, spi_master->bits_per_word_mask);
  if ( (spi_device = spi_new_device( spi_master, &spi_device_info )) == 0 ) {
    INFO("%s", "spi_new_device(...) failed");
    return -ENODEV;
  }
  spi_device->bits_per_word = spi_bits_per_word;
  if ( (ret = spi_setup(spi_device)) != 0 ) {
    spi_unregister_device(spi_device);
    return ret;
  }
  return 0;
}

void portex_spi_free(void)
{
  spi_unregister_device(spi_device);
}

//spi_write( spi_device, &write_data, sizeof write_data );
