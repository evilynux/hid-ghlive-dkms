KERNEL_SOURCE_DIR ?= "/lib/modules/$(shell uname -r)/build"

LD := ld.bfd

all: modules

clean modules modules_install:
	$(MAKE) -C $(KERNEL_SOURCE_DIR) INSTALL_MOD_DIR="kernel/drivers/hid" LD=$(LD) M=$(shell pwd)/src $@

reinstall: modules
	sudo make modules_install
	sudo rmmod hid-ghlive || true
	sudo modprobe hid-ghlive
	#sudo insmod $(shell pwd)/src/hid-ghlive.ko # Alternatively...
