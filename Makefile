
obj-m := irq_intro.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

all: modules

modules clean:
	$(MAKE) -C $(KERNELDIR) M=$(shell pwd) $@