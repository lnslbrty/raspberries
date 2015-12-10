/**
 * @file   pe_mod.c
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

static unsigned short shift_regs = 1;
static unsigned short shift_pins = 7;

module_param(shift_regs, short, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(shift_regs, "Shift register count");
module_param(shift_pins, short, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(shift_pins, "Shift register pin count");


/* What should happen, when this module gets loaded? */
static int __init pe_mod_init(void)
{
  INFO("%s", "init");
  INFO("shift_regs=%u, shift_pins=%u", shift_regs, shift_pins);
  if (portex_sysfs_init()) {
    INFO("%s", "sysfs init failed");
    return 1;
  }
  return 0;
}

/* What should happen, when this module gets unloaded? */
static void __exit pe_mod_exit(void)
{
  portex_sysfs_free();
  INFO("%s", "unloaded");
}


/* the lnker needs to know the entry/exit function */
module_init(pe_mod_init);
module_exit(pe_mod_exit);
