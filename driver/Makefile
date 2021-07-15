
KERNEL_VERSION = $(shell uname -r)
#KERNEL_VERSION = 4.9.0-6-rpi

TARGET_MODULE = chipbee

obj-m += chipbee.o
chipbee-objs := main.o usb.o tty.o


all:
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(KERNEL_VERSION)/build M=$(PWD) clean

load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko

