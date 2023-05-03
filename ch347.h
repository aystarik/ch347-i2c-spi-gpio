/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for CH347 driver
 */

#include <linux/usb.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>

#define DEFAULT_TIMEOUT 1000	/* 1s USB requests timeout */

#define SEG_SIZE 512

/* Number of SPI devices the device can control */
#define CH347_SPI_MAX_NUM_DEVICES 2

struct CH347_device {
	struct usb_device *usb_dev;
	struct usb_interface *iface;
	struct mutex usb_lock;

    int ep_in;
    int ep_out;

    u8 ibuf[SEG_SIZE];
    u8 obuf[SEG_SIZE];

    /* I2C */
	struct i2c_adapter adapter;
	bool i2c_init;

#if 0
	/* GPIO */
	struct gpio_chip gpio;
	struct mutex gpio_lock;
	bool gpio_init;
	u16 gpio_dir;		/* 1 bit per pin, 0=IN, 1=OUT. */
	u16 gpio_last_read;	/* last GPIO values read */
	u16 gpio_last_written;	/* last GPIO values written */
	u8 gpio_buf[SEG_SIZE];

	struct {
		char name[32];
		bool enabled;
		struct irq_chip irq;
		int num;
		struct urb *urb;
		struct usb_anchor urb_out;
        u8 buf[CH347_USB_MAX_INTR_SIZE];
	} gpio_irq;

	/* SPI */
	struct spi_master *master;
	struct mutex spi_lock;
	u8 cs_allocated;	/* bitmask of allocated CS for SPI */
	struct gpio_desc *spi_gpio_core_desc[3];
	struct spi_client {
		struct spi_device *slave;
		struct gpio_desc *gpio;
        u8 buf[CH347_SPI_NSEGS * SEG_SIZE];
    } spi_clients[CH347_SPI_MAX_NUM_DEVICES];
#endif
};

void CH347_i2c_remove(struct CH347_device *dev);
int CH347_i2c_init(struct CH347_device *dev);
#if 0
void CH347_gpio_remove(struct CH347_device *dev);
int CH347_gpio_init(struct CH347_device *dev);
void CH347_spi_remove(struct CH347_device *dev);
int CH347_spi_init(struct CH347_device *dev);
#endif
