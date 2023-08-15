// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the CH341T USB to I2C adapter
 *
 * Copyright 2021, Frank Zago
 * Copyright (c) 2017 Gunar Schorcht (gunar@schorcht.net)
 * Copyright (c) 2016 Tse Lun Bien
 * Copyright (c) 2014 Marco Gittler
 * Copyright (c) 2006-2007 Till Harbaum (Till@Harbaum.org)
 *
 * The full UART functionality is handled by the CDC ACM driver
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "ch347.h"

static void CH347_usb_free_device(struct CH347_device *dev)
{
#if 0
    CH347_spi_remove(dev);
#endif
    ch347_gpio_remove(dev);
    CH347_i2c_remove(dev);

	usb_set_intfdata(dev->iface, NULL);
	usb_put_dev(dev->usb_dev);

	kfree(dev);
}

static int CH347_usb_probe(struct usb_interface *iface, const struct usb_device_id *usb_id)
{
    struct usb_endpoint_descriptor *ep;
    struct usb_host_interface *iface_desc;
    struct CH347_device *dev;
    int rc, i;

    dev = kzalloc(sizeof(struct CH347_device), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->usb_dev = usb_get_dev(interface_to_usbdev(iface));
	dev->iface = iface;
    iface_desc = iface->cur_altsetting;
    if (iface_desc->desc.bNumEndpoints < 1)
            return -ENODEV;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        ep = &iface_desc->endpoint[i].desc;

        if ((ep->bEndpointAddress & USB_DIR_IN) && (ep->bmAttributes & 3) == 2) {
            dev->ep_in = ep->bEndpointAddress;
            dev_info(&iface->dev, "USB bulk in ep 0x%02x", dev->ep_in);
        }

        if ((ep->bEndpointAddress & USB_DIR_IN) == 0 && (ep->bmAttributes & 3) == 2) {
            dev->ep_out = ep->bEndpointAddress;
            dev_info(&iface->dev, "USB bulk out ep 0x%02x", dev->ep_out);
        }
    }

	mutex_init(&dev->usb_lock);

	usb_set_intfdata(iface, dev);

    rc = CH347_i2c_init(dev);
	if (rc)
		goto free_dev;
    rc = ch347_gpio_init(dev);
	if (rc)
		goto rem_i2c;

#if 0
    rc = CH347_spi_init(dev);
	if (rc)
		goto rem_gpio;
#endif
	return 0;

    ch347_gpio_remove(dev);

rem_i2c:
    CH347_i2c_remove(dev);

free_dev:
	usb_put_dev(dev->usb_dev);
	kfree(dev);

	return rc;
}

static void CH347_usb_disconnect(struct usb_interface *usb_if)
{
    struct CH347_device *dev = usb_get_intfdata(usb_if);

    CH347_usb_free_device(dev);
}

static const struct usb_device_id CH347_usb_table[] = {
    { USB_DEVICE_INTERFACE_NUMBER(0x1a86, 0x55db, 0x02) }, /* CH347 Mode 1 SPI+IIC+UART */
    { USB_DEVICE_INTERFACE_NUMBER(0x1a86, 0x55dd, 0x02) }, /* CH347 Mode 3 JTAG+IIC+UART */
    { }
};
MODULE_DEVICE_TABLE(usb, CH347_usb_table);

static struct usb_driver CH347_usb_driver = {
    .name       = "CH347-buses",
    .id_table   = CH347_usb_table,
    .probe      = CH347_usb_probe,
    .disconnect = CH347_usb_disconnect
};

module_usb_driver(CH347_usb_driver);

MODULE_AUTHOR("Various");
MODULE_DESCRIPTION("CH347 USB to I2C/SPI/GPIO adapter");
MODULE_LICENSE("GPL v2");
