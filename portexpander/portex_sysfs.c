#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>

#include "portex.h"


static struct kobject *ko_portex = NULL;
static int test = 1;


static ssize_t test_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
  return sprintf(buf, "%d\n", test);
}

static ssize_t test_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
  sscanf(buf, "%du", &test);
  return count;
}

static struct kobj_attribute my_attr = __ATTR(test, 0644, test_show, test_store);

int portex_sysfs_init(void)
{
  int err = 0;

  ko_portex = kobject_create_and_add("pe_kobject", kernel_kobj);
  if (!ko_portex)
    return -ENOMEM;
  err = sysfs_create_file(ko_portex, &my_attr.attr);
  if (err)
    INFO("%s", "sysfs error");

  return err;
}

void portex_sysfs_free(void)
{
  kobject_put(ko_portex);
}
