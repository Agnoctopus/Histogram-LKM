# Kbuild information
modname = histogram
obj-m = $(modname).o

# Kernel version
KVERSION = $(shell uname -r)

# Path of the kernel source directory
KDIR = /lib/modules/$(KVERSION)/build

# Module kernel object
MOD = $(modname).ko


all: modules

modules:
	make -C $(KDIR) M=$(PWD) $(MOD)


install:
	make -C $(KDIR) M=$(PWD) modules_install
	modprobe $(modname)

uninstall:
	modprobe -r histogram
	$(RM) $(KDIR)/extra/$(modname)

load:
	-rmmod $(MOD)
	insmod $(MOD)

unload:
	-rmmod $(MOD)

doc:
	doxygen

clean:
	make -C $(KDIR) M=$(PWD) clean
	$(RM) doc

.PHONY: all modules install uninstall load unload doc clean
