obj-m += silk.o

ccflags-y := -O2 -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4

KERNELVER = $(shell uname -r)
KDIR      = /lib/modules/$(shell uname -r)/build
PWD		  = $(shell pwd)

all:
	export PACKAGE_VERSION=${PACKAGE_VERSION}
	./silk-helper.py > config.h
	make -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	make -C $(KDIR) M=$(PWD) modules_install
	/sbin/depmod -a $(KERNELVER)
