
ramindex-objs := \
    ramindex-main.o \
    ramindex-cortex-a72.o \
    ramindex-cortex-a720.o

obj-m := ramindex.o

ifeq ($(KERNELRELEASE),)

KERNELRELEASE := `uname -r`
KDIR := /lib/modules/$(KERNELRELEASE)/build

.PHONY: all install clean distclean
.PHONY: ramindex.ko

all: ramindex.ko

ramindex.ko:
	@echo "Building $@ driver..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	rm -f *~
	rm -f Module.symvers Module.markers modules.order
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif # !KERNELRELEASE
