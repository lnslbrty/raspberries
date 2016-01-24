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


static ssize_t gpio_all_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t gpio_all_get(struct class *cls, struct class_attribute *attr, char *buf);
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
  __ATTR(gpio_all, S_IRUGO | S_IWUSR , gpio_all_get, gpio_all_set),
  __ATTR_NULL
};

static struct class portex_class =
{
  .name = DRIVER_NAME,
  .owner = THIS_MODULE,
  .class_attrs = portex_class_attrs
};


static unsigned int get_gpio(struct class_attribute *a)
{
  unsigned int idx;
  sscanf(a->attr.name, "gpio%u", &idx);
  return idx;
}

/* not implemented */
static ssize_t gpio_all_get(struct class *cls, struct class_attribute *attr, char *buf)
{
  return 0;
}

static ssize_t gpio_all_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
  int value;

  if ( sscanf(buf, "%d", &value) == 1 ) {
    portex_write_cached(MCP23S08_REG_GPIO, 0xFF, value);
    portex_write_cached(MCP23S17_REG_GPIO, 0xFF, value);
  }
  return count;
}

static ssize_t gpio_get(struct class *cls, struct class_attribute *attr, char *buf)
{
  int ret;
  enum MCP23S_REGS reg;
  unsigned int gpio = get_gpio(attr);

  if (gpio > 7) {
    reg   = MCP23S17_REG_GPIO;
    gpio -= 8;
  } else {
    reg   = MCP23S08_REG_GPIO;
  }
  ret = portex_read_cached(reg, gpio);
  return sprintf(buf, "%d\n", ret);
}

static ssize_t gpio_set(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
  int value;
  enum MCP23S_REGS reg;
  unsigned int gpio = get_gpio(attr);

  if ( sscanf(buf, "%d", &value) == 1 ) {
    if (gpio > 7) {
      reg   = MCP23S17_REG_GPIO;
      gpio -= 8;
    } else {
      reg   = MCP23S08_REG_GPIO;
    }
    portex_write_cached(reg, (0x1 << gpio), value);
  }
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
