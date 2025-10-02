obj-m += jamvox.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD)/src modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src clean

install:
	install -D -m 644 src/jamvox.ko $(DESTDIR)/lib/modules/$(KVERSION)/extra/jamvox.ko

.PHONY: all clean install
