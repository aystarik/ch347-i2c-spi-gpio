// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO interface for the CH347.
 *
 * Copyright 2023, Alexey Starikovskiy <aystarik@gmail.com>
 */

#include "ch347.h"

#define CH347_GPIO_NUM_PINS         8    /* Number of GPIO pins */

static void ch347_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct CH347_device *dev = gpiochip_get_data(chip);
	for (unsigned i = 0; i < 8; ++i) {
		u8 pin = dev->gpio_ibuf[3 + i];
		seq_printf(s, "pin[%d](%s):%d", i, pin & 0x80 ? "out":"in",
			pin & 0x40 ? 1 : 0);
	}
}

static int gpio_transfer(struct CH347_device *dev)
{
	int actual;
	int rc;

	rc = usb_bulk_msg(dev->usb_dev, usb_sndbulkpipe(dev->usb_dev, dev->ep_out),
			  dev->gpio_obuf, 11, &actual, DEFAULT_TIMEOUT);
	if (rc < 0)
		return rc;

	rc = usb_bulk_msg(dev->usb_dev,
			  usb_rcvbulkpipe(dev->usb_dev, dev->ep_in),
			  dev->gpio_ibuf, 11, &actual, DEFAULT_TIMEOUT);

	return rc;
}

static int ch347_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	int rc;
	struct CH347_device *dev = gpiochip_get_data(chip);
	if (offset > 7) return 0;
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	rc = gpio_transfer(dev);
	if (rc)
		return rc;

	return (dev->gpio_ibuf[3 + offset] & 0x40) ? 1 : 0;
}

static int ch347_gpio_get_direction(struct gpio_chip *chip,
				    unsigned int offset)
{
	int rc;
	struct CH347_device *dev = gpiochip_get_data(chip);
	if (offset > 7) return 0;
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	rc = gpio_transfer(dev);
	if (rc)
		return rc;

	return (dev->gpio_ibuf[3 + offset] & 0x80) ? 1 : 0;
}

static int ch347_gpio_get_multiple(struct gpio_chip *chip,
				   unsigned long *mask, unsigned long *bits)
{
	int rc;
	struct CH347_device *dev = gpiochip_get_data(chip);
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	rc = gpio_transfer(dev);

	if (rc)
		return rc;

	for (unsigned i = 0; i < 8; ++i) {
		if (*mask & BIT(i) && (dev->gpio_ibuf[3 + i] & 0x40)) {
			*bits |= BIT(i);
		}
	}

	return 0;
}

static void ch347_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct CH347_device *dev = gpiochip_get_data(chip);
	if (offset > 7) return;
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	dev->gpio_obuf[3 + offset] |= 0xc0; // enable pin change
	if (dev->gpio_ibuf[3 + offset] & 0x80) { // copy direction
		dev->gpio_obuf[3 + offset] |= 0x30;
	}
	if (value) {
		dev->gpio_obuf[3 + offset] |= 0x08;
	}
	gpio_transfer(dev);
}

static void ch347_gpio_set_multiple(struct gpio_chip *chip,
				    unsigned long *mask, unsigned long *bits)
{
	struct CH347_device *dev = gpiochip_get_data(chip);
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	for (unsigned i = 0; i < 8; ++i) {
		if (*mask & BIT(i) && (dev->gpio_ibuf[3 + i] & 0x80)) {
			dev->gpio_obuf[3 + i] |= 0xc0; // enable pin change
			if (*bits & BIT(i)) {
				dev->gpio_obuf[3 + i] |= 0x08;
			}
		} 
	}
	gpio_transfer(dev);
}

static int ch347_gpio_direction_input(struct gpio_chip *chip,
				      unsigned int offset)
{
	struct CH347_device *dev = gpiochip_get_data(chip);
	if (offset > 7) return 0;
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	dev->gpio_obuf[3 + offset] |= 0xc0; // enable pin change
	if (dev->gpio_ibuf[3 + offset] & 0x40) { // copy value
		dev->gpio_obuf[3 + offset] |= 0x08;
	}
	gpio_transfer(dev);

	return 0;
}

static int ch347_gpio_direction_output(struct gpio_chip *chip,
				       unsigned int offset, int value)
{
	struct CH347_device *dev = gpiochip_get_data(chip);
	if (offset > 7) return 0;
	memset(dev->gpio_obuf + 3, 0, 8); // clear all pins
	dev->gpio_obuf[3 + offset] |= 0xc0; // enable pin change
	if (dev->gpio_ibuf[3 + offset] & 0x40) { // copy value
		dev->gpio_obuf[3 + offset] |= 0x08;
	}
	dev->gpio_obuf[3 + offset] |= 0x30; // set direction == output
	gpio_transfer(dev);

	return 0;
}

void ch347_gpio_remove(struct CH347_device *dev)
{
	gpiochip_remove(&dev->gpio);
}

int ch347_gpio_init(struct CH347_device *dev)
{
	struct gpio_chip *gpio = &dev->gpio;
	int rc;

	gpio->label = "ch347";
	gpio->parent = &dev->usb_dev->dev;
	gpio->owner = THIS_MODULE;

	gpio->get = ch347_gpio_get;
	gpio->get_multiple = ch347_gpio_get_multiple;

	gpio->get_direction = ch347_gpio_get_direction;
	gpio->direction_input = ch347_gpio_direction_input;
	gpio->direction_output = ch347_gpio_direction_output;

	gpio->set = ch347_gpio_set;
	gpio->set_multiple = ch347_gpio_set_multiple;

	gpio->dbg_show = ch347_gpio_dbg_show;

	gpio->base = -1;
	gpio->ngpio = CH347_GPIO_NUM_PINS;
	gpio->can_sleep = true;
	
	memset(dev->gpio_obuf, 0, 11);
	dev->gpio_obuf[0] = 0xcc; // these fields do not ever change 
	dev->gpio_obuf[1] = 8; // these fields do not ever change 
	dev->gpio_obuf[2] = 0; // these fields do not ever change 

	gpio_transfer(dev);

	rc = gpiochip_add_data(gpio, dev);
	if (rc) {
		dev_err(&dev->usb_dev->dev, "Could not add GPIO\n");
		return rc;
	}
	return 0;
}
