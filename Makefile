obj-m += silk.o

ccflags-y := -O2 -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4

KERNELVER	?= $(shell uname -r)
KERNELDIR	?= /lib/modules/$(KERNELVER)/build
PWD		:= $(shell pwd)

ifneq ($(KERNELRELEASE),)
    KERNELDIR := /lib/modules/$(KERNELRELEASE)/build
else
    ## KERNELRELEASE not set.
    KERNELDIR := /lib/modules/$(KERNELVER)/build
endif


all:
	export PACKAGE_VERSION=${PACKAGE_VERSION}
	./silk-helper.py > config.h
	make -C $(KERNELDIR) M=$(PWD)

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install:
	make -C $(KERNELDIR) M=$(PWD) modules_install
	/sbin/depmod -a $(KERNELVER)
