#ifndef PORTEX_H
#define PORTEX_H 1

#include <linux/printk.h>


#define PRINTP "portex: "
static const char prefix[] = PRINTP;
#define INFO(fmt, ...) printk(KERN_INFO "%s" fmt "\n", prefix, __VA_ARGS__)


int portex_sysfs_init(void);

void portex_sysfs_free(void);

int portex_spi_init(void);

void portex_spi_free(void);

#endif
