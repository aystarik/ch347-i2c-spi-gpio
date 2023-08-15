PWD         := $(shell pwd)
KVERSION    := $(shell uname -r)
KERNEL_DIR  ?= /lib/modules/$(KVERSION)/build

obj-m       := ch347-buses.o

ch347-buses-objs := ch347-core.o ch347-i2c.o ch347-gpio.o
#ch347-spi.o

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
