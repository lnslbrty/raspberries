obj-$(CONFIG_PORTEX) := portex.o
portex-objs := portex_init.o portex_sysfs.o portex_spi.o
ccflags-y := -Wall


all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
