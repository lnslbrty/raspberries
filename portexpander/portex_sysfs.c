#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/string.h>

#include "portex.h"


// Store and Show functions……
static ssize_t attr1_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count);
static ssize_t attr1_show(struct class *cls, struct class_attribute *attr, char *buf);

// Attributes declaration: Here I have declared only one attribute attr1
static struct class_attribute pwm_class_attrs[] = {
  __ATTR(attr1, S_IRUGO | S_IWUSR , attr1_show, attr1_store), // use macro for permission
  __ATTR_NULL
};

//Struct class is basic struct needed to create classs attributes.”dev_jes” is the directory under /sys/class.
static struct class pwm_class =
{
  .name = "portex",
  .owner = THIS_MODULE,
  .class_attrs = pwm_class_attrs
};

static unsigned int test = 0;


// class attribute show function.
static ssize_t attr1_show(struct class *cls, struct class_attribute *attr, char *buf)
{
  return sprintf(buf, "%u\n", test);
}

// class attribute store function
static ssize_t attr1_store(struct class *cls, struct class_attribute *attr, const char *buf, size_t count)
{
  sscanf(buf, "%u", &test);
  return count;
}

int portex_sysfs_init(void)
{
  class_register(&pwm_class);
  return 0;
}

void portex_sysfs_free(void)
{
  class_unregister(&pwm_class);
}
