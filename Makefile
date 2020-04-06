modname = histogram
obj-m = histogram.o


KVERSION = $(shell uname -r)
MODDIR = /lib/modules/$(KVERSION)/build

KOBJ = $(modname).ko

all:
	make -C $(MODDIR) M=$(PWD) modules

clean:
	make -C $(MODDIR) M=$(PWD) clean

load:
	-rmmod $(KOBJ)
	insmod $(KOBJ).ko

unload:
	-rmmod $(KOBJ)

test:
	-rmmod $(KOBJ).ko
	sudo dmesg -C
	sudo insmod $(KOBJ).ko
	dmesg
