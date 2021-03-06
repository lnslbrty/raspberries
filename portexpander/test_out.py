#!/usr/bin/python

import sys
import time
import random


SYSFS_PREFIX="/sys/class/portex"
GPIO_COUNT=6
GPIO_PER_PORT=8
GPIOA_FDS=list()
GPIOB_FDS=list()


def ledout(index, value):
	GPIOA_FDS[index].write(str(value))
	GPIOA_FDS[index].flush()
	GPIOB_FDS[index].write(str(value))
	GPIOB_FDS[index].flush()

def ledoutp(index, value, porta):
	if porta:
		GPIOA_FDS[index].write(str(value))
		GPIOA_FDS[index].flush()
	else:
		GPIOB_FDS[index].write(str(value))
		GPIOB_FDS[index].flush()

def ledall(value):
	fd.write("0")
	fd.flush()

def blinkTest0(speed):
	for i in range(GPIO_COUNT):
		ledout(i,1)
		time.sleep(speed)
		ledall(0)

def blinkTest1(speed):
	blinkTest0(speed)
        for i in range(GPIO_COUNT-1, -1, -1):
		ledout(i,1)
		time.sleep(speed)
		ledall(0)

def blinkTest2(speed):
	for i in range(GPIO_COUNT):
		ledoutp(i,1,True)
		ledoutp(i,1,False)
		time.sleep(speed)
	for i in range(GPIO_COUNT):
		ledoutp(i,0,True)
		ledoutp(i,0,False)
		time.sleep(speed)

def blinkTest3(speed, extra):
	for i in range(GPIO_COUNT):
		ledoutp(i,1,True)
		time.sleep(speed)
		if extra is False:
			ledoutp(i,0,True)
	for i in range(GPIO_COUNT):
		ledoutp(i,1,False)
		time.sleep(speed)
		if extra is False:
			ledoutp(i,0,False)

def blinkTest4(speed, count):
	for i in range(count):
		pin = int((random.random() * 16) % GPIO_COUNT)
		prt = int((random.random() * 8) % 2)
		if prt == 0:
			use_port = False
		else:
			use_port = True
		ledoutp(pin,1,use_port)
		time.sleep(speed)
		ledoutp(pin,0,use_port)
		time.sleep(speed*5/3)

def GPIOinit():
	print 'GPIOinit'
	for i in range(GPIO_COUNT):
		GPIOA_FDS.append(open(SYSFS_PREFIX + "/gpio" + str(i), "w"))
		GPIOB_FDS.append(open(SYSFS_PREFIX + "/gpio" + str(i+GPIO_PER_PORT), "w"))

def GPIOfree():
	print 'GPIOfree'
	for i in range(GPIO_COUNT):
		GPIOA_FDS[i].close()
		GPIOA_FDS[i].close()


# main
print '[Starting]'
print 'SysFS Prefix:', SYSFS_PREFIX

print 'Selftest .. ', SYSFS_PREFIX + '/gpio_all'
fd = open(SYSFS_PREFIX + "/gpio_all", "w")
fd.write("1")
fd.flush()
time.sleep(1)
fd.write("0")
fd.flush()
time.sleep(0.5)

GPIOinit()
count=0
try:
	while True:
		for i in range(3):
			blinkTest0(0.1)
		for i in range(3):
			blinkTest1(0.1)
		for i in range(2):
			blinkTest2(0.2)
		for i in range(5):
			blinkTest2(0.05)
			blinkTest2(0.05)
		for i in range(5):
			blinkTest3(0.05, True)
			blinkTest3(0.05, False)
		for i in range(10):
			blinkTest3(0.05, False)
		for i in range(5):
			blinkTest4(0.075/(i+1), 50)
		print '[' + str(count) + ']',
		count += 1
		sys.stdout.flush()
except KeyboardInterrupt:
	GPIOfree()
	fd.write("0")
	fd.flush()
	fd.close()
	print 'done'
