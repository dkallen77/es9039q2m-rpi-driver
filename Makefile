# Makefile for ES9039Q2M kernel modules

obj-m += es9039q2m-i2c.o
obj-m += es9039q2m-machine.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	install -v -m 644 es9039q2m-i2c.ko /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 es9039q2m-machine.ko /lib/modules/$(shell uname -r)/extra/
	depmod -a