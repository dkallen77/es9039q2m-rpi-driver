# Makefile for ES9039Q2M kernel modules

obj-m += es9039q2m-i2c.o
obj-m += es9039q2m-machine.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
BUILDDIR := $(PWD)/build
DTB_SRC := es9039q2m-overlay.dts
DTB_BIN := es9039q2m-overlay.dtbo

all: modules dtb

modules:
	@mkdir -p $(BUILDDIR)
	$(MAKE) -C $(KDIR) M=$(PWD) O=$(BUILDDIR) modules

dtb:
	dtc -@ -I dts -O dtb -o $(DTB_BIN) $(DTB_SRC)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) O=$(BUILDDIR) clean
	@rm -rf $(BUILDDIR)
	@rm -f $(DTB_BIN)

install:
	install -v -m 644 build/es9039q2m-i2c.ko /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 build/es9039q2m-machine.ko /lib/modules/$(shell uname -r)/extra/
	install -v -m 644 es9039q2m-overlay.dtbo /boot/overlays/
	depmod -a