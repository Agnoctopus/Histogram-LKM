modname = histogram
obj-m = histogram.o


KVERSION = $(shell uname -r)
MODDIR = /lib/modules/$(KVERSION)/build

MOD = $(modname).ko

all:
	make -C $(MODDIR) M=$(PWD) modules

clean:
	make -C $(MODDIR) M=$(PWD) clean

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
