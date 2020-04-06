# kbuild information
modname = histogram
obj-m = $(modname).o
$(modname)-y =


KVERSION = $(shell uname -r)

# The path of the kernel source directory
KDIR = /lib/modules/$(KVERSION)/build

MOD = $(modname).ko

all: modules

modules:
	make -C $(KDIR) M=$(PWD) $(MOD)

clean:
	make -C $(KDIR) M=$(PWD) clean

load:
	-rmmod $(MOD)
	insmod $(MOD)

unload:
	-rmmod $(MOD)

test:
	-sudo rmmod $(MOD)
	sudo dmesg -C
	sudo insmod $(MOD)
	dmesg

doc:
	doxygen

install:
	make -C $(KDIR) M=$(PWD) modules_install
	modprobe $(modname)

uninstall:
	modprobe -r histogram
	$(RM) $(KDIR)/extra/$(modname)

PHONY: all modules clean load unload doc test
