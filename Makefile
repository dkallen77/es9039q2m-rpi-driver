# Simple Makefile for ES9039Q2M kernel modules and DTB overlay

obj-m += es9039q2m-i2c.o
obj-m += es9039q2m-machine.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
DTB_SRC := es9039q2m-overlay.dts
DTB_BIN := es9039q2m-overlay.dtbo

.PHONY: all modules dtb clean install

all: modules dtb

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

dtb: $(DTB_BIN)

$(DTB_BIN): $(DTB_SRC)
	dtc -@ -I dts -O dtb -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -f *.o *.ko *.mod.c *.order *.symvers *.cmd modules.order Module.symvers *.dtbo

install: all
	@mkdir -p /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 es9039q2m-i2c.ko /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 es9039q2m-machine.ko /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 es9039q2m-overlay.dtbo /boot/overlays/
	depmod -a