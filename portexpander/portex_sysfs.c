/**
 * @file   portex_sysfs.c
 * @author Toni Uhlig, Eric Mueller
 * @date   10.12.2015
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/string.h>

#include "portex.h"


static ssize_t gpio_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t gpio_get(struct class *cls, struct class_attribute *attr, char *buf);
static ssize_t gpio_dir_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t gpio_dir_get(struct class *cls, struct class_attribute *attr, char *buf);


#define GPIO(name) \
   __ATTR(gpio ## name,         S_IRUGO | S_IWUSR , gpio_get,     gpio_set), \
   __ATTR(gpio ## name ## _dir, S_IRUGO | S_IWUSR , gpio_dir_get, gpio_dir_set)
static struct class_attribute portex_class_attrs[] = {
  GPIO(0), GPIO(1), GPIO(2),  GPIO(3),  GPIO(4),  GPIO(5),  GPIO(6),  GPIO(7),
  GPIO(8), GPIO(9), GPIO(10), GPIO(11), GPIO(12), GPIO(13), GPIO(14), GPIO(15),
  __ATTR_NULL
};

static struct class portex_class =
{
  .name = DRIVER_NAME,
  .owner = THIS_MODULE,
  .class_attrs = portex_class_attrs
};

#define MAX_GPIO 16
static unsigned int gpio_values[MAX_GPIO];
static unsigned int test = 0;

static unsigned int get_gpio_pin(struct class_attribute *a)
{
  unsigned int idx;
  sscanf(a->attr.name, "gpio%u", &idx);
  return idx;
}

static unsigned int encode_gpio_pin(unsigned int pin)
{
  if (pin > 7)
    pin -= 8;
  return (0x01 << pin);
}

static ssize_t gpio_get(struct class *cls, struct class_attribute *attr, char *buf)
{
  printk("get....: %u\n", get_gpio_pin(attr));
  printk("encode.: %u\n", encode_gpio_pin(get_gpio_pin(attr)));
  return sprintf(buf, "%u\n", test);
}

static ssize_t gpio_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
  sscanf(buf, "%u", &test);
  return count;
}

/* TODO: gpio direction ( IN <---> OUT ) */
static ssize_t gpio_dir_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
  return 0;
}

static ssize_t gpio_dir_get(struct class *cls, struct class_attribute *attr, char *buf)
{
  return 0;
}

int portex_sysfs_init(void)
{
  class_register(&portex_class);
  return 0;
}

void portex_sysfs_free(void)
{
  class_unregister(&portex_class);
}
