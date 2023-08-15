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

	struct gpio_chip gpio;
	u8 gpio_ibuf[3 + 8];
	u8 gpio_obuf[3 + 8];

    /* I2C */
	struct i2c_adapter adapter;
	bool i2c_init;

#if 0
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
void ch347_gpio_remove(struct CH347_device *dev);
int ch347_gpio_init(struct CH347_device *dev);
#if 0
void CH347_spi_remove(struct CH347_device *dev);
int CH347_spi_init(struct CH347_device *dev);
#endif
