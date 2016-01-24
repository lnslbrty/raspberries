/**
 * @file   portex_init.c
 * @author Toni Uhlig, Eric Mueller
 * @date   08.12.2015
 */

#include <linux/kernel.h>
#include <linux/init.h>		// basic macro definitions, generated while your kernel was compiled
#include <linux/module.h>	// basic macro/function/struct definitions for kernel modules like MODULE_*
#include <linux/moduleparam.h>	// kernel module parameter macros/functions

#include "portex.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Toni Uhlig, Eric Mueller");
MODULE_DESCRIPTION("A GPIO based PortExpander for Linux");
MODULE_VERSION("0.1");


/* What should happen, when this module gets loaded? */
static int __init pe_mod_init(void)
{
  int ret;

  pr_info("%s: %s()\n", DRIVER_NAME, __func__);
  if ( (ret = portex_spi_init()) != 0 ) {
    return ret;
  }
  if ( (ret = portex_sysfs_init()) != 0 ) {
    return ret;
  }
  return 0;
}

/* What should happen, when this module gets unloaded? */
static void __exit pe_mod_exit(void)
{
  pr_info("%s: %s()\n", DRIVER_NAME, __func__);
  portex_sysfs_free();
  portex_spi_free();
}


/* the lnker needs to know the entry/exit function */
module_init(pe_mod_init);
module_exit(pe_mod_exit);
