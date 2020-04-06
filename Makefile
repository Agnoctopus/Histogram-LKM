modname = histogram
obj-m = histogram.o


KVERSION = $(shell uname -r)

# The path of the kernel source directory
KDIR = /lib/modules/$(KVERSION)/build

MOD = $(modname).ko

all: modules

modules:
	make -C $(KDIR) M=$(PWD) modules

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
