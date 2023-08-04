// SPDX-License-Identifier: GPL-2.0
/*
 * I2C interface for the CH347T.
 *
 * Copyright 2021, Frank Zago
 * Copyright (c) 2016 Tse Lun Bien
 * Copyright (c) 2014 Marco Gittler
 * Copyright (C) 2006-2007 Till Harbaum (Till@Harbaum.org)
 */

#include "ch347.h"

enum ch347_speed_t {
    CH347_I2C_LOW_SPEED = 0,   // low speed - 20kHz
    CH347_I2C_STANDARD_SPEED = 1,      // standard speed - 100kHz
    CH347_I2C_FAST_SPEED = 2,  // fast speed - 400kHz
    CH347_I2C_HIGH_SPEED = 3,  // high speed - 750kHz
};

#define CH347_CMD_I2C_STREAM 0xAA
#define CH347_CMD_I2C_STM_END 0x00

#define CH347_CMD_I2C_STM_STA 0x74
#define CH347_CMD_I2C_STM_STO 0x75
#define CH347_CMD_I2C_STM_OUT 0x80
#define CH347_CMD_I2C_STM_IN  0xC0
#define CH347_CMD_I2C_STM_SET 0x60

#define CH347_MAX_I2C_XFER 0x3f

static int CH347_usb_transfer(struct CH347_device *CH347_dev, int out_len, int in_len)
{
    int retval;
    int actual;

    //DEV_INFO (CH347_IF_ADDR, "bulk_out %d bytes, bulk_in %d bytes", out_len, (in_len == 0) ? 0 : CH347_USB_MAX_BULK_SIZE);

    retval = usb_bulk_msg(CH347_dev->usb_dev, usb_sndbulkpipe(CH347_dev->usb_dev, CH347_dev->ep_out),
                            CH347_dev->obuf, out_len, &actual, DEFAULT_TIMEOUT);
    if (retval < 0)
        return retval;

    if (in_len == 0)
        return actual;

    memset(CH347_dev->ibuf, 0, sizeof(CH347_dev->ibuf));
    retval = usb_bulk_msg(CH347_dev->usb_dev, usb_rcvbulkpipe(CH347_dev->usb_dev, CH347_dev->ep_in),
                            CH347_dev->ibuf, SEG_SIZE, &actual, DEFAULT_TIMEOUT);

    if (retval < 0)
        return retval;
    return actual;
}

int CH347_i2c_read(struct CH347_device *dev, struct i2c_msg *msg)
{
    int byteoffset = 0, bytestoread;
    int ret = 0;
    uint8_t *ptr;
    while (msg->len - byteoffset > 0) {
        mutex_lock(&dev->usb_lock);

        bytestoread = msg->len - byteoffset;
        if (bytestoread > CH347_MAX_I2C_XFER)
            bytestoread = CH347_MAX_I2C_XFER;
        ptr = dev->obuf;
        *ptr++ = CH347_CMD_I2C_STREAM;
        *ptr++ = CH347_CMD_I2C_STM_STA;
        *ptr++ = CH347_CMD_I2C_STM_OUT | 1;
        *ptr++ = (msg->addr << 1) | 1;
        if (bytestoread > 1) {
            *ptr++ = CH347_CMD_I2C_STM_IN | (bytestoread - 1);
        }
        *ptr++ = CH347_CMD_I2C_STM_IN;
        *ptr++ = CH347_CMD_I2C_STM_STO;
        *ptr++ = CH347_CMD_I2C_STM_END;

        ret = CH347_usb_transfer(dev, ptr - dev->obuf, SEG_SIZE);
        if (ret < 1)
            goto unlock;
        if (ret != bytestoread + 1)
            ret = -1;
        if (dev->ibuf[0] != 1) // check status for NACK
            ret = -ETIMEDOUT;
        if (ret > -1) {
            memcpy(&msg->buf[byteoffset], &dev->ibuf[1], bytestoread);
            byteoffset += bytestoread;
        }
unlock:
        mutex_unlock(&dev->usb_lock);

        if (ret < 0)
            break;
    }
    return ret;
}

int CH347_i2c_write(struct CH347_device *dev, struct i2c_msg *msg) {
    unsigned left = msg->len, wlen;
    uint8_t *ptr = msg->buf, *outptr;
    int ret = 0, i;
    bool first = true;
    do {
        mutex_lock(&dev->usb_lock);

        outptr = dev->obuf;
        *outptr++ = CH347_CMD_I2C_STREAM;
        wlen = left;
        if (wlen > CH347_MAX_I2C_XFER - 1)
            wlen = CH347_MAX_I2C_XFER - 1;
        if (first) { // Start packet
            *outptr++ = CH347_CMD_I2C_STM_STA;
            *outptr++ = CH347_CMD_I2C_STM_OUT | (wlen + 1);
            *outptr++ = msg->addr << 1;
        } else {
            *outptr++ = CH347_CMD_I2C_STM_OUT | wlen;
        }
        memcpy(outptr, ptr, wlen);
        outptr += wlen;
        ptr += wlen;
        left -= wlen;
        if (left == 0) {  // Stop packet
            *outptr++ = CH347_CMD_I2C_STM_STO;
        }
        *outptr = CH347_CMD_I2C_STM_END;
        first = false;
        ret = CH347_usb_transfer(dev, outptr - dev->obuf, SEG_SIZE);
        if (ret > 0) {
            for (i = 0; i < ret; ++i) {
                if (dev->ibuf[i] != 1) {
                    ret = -ETIMEDOUT;
                    break;
                }
            }
        }
        mutex_unlock(&dev->usb_lock);

        if (ret < 0)
            break;
    } while (left);

    return ret;
}

#define CHECK_PARAM_RET(cond,err) if (!(cond)) return err;

static int CH347_i2c_xfer(struct i2c_adapter *adpt, struct i2c_msg *msgs, int num)
{
    struct CH347_device *dev;
    int result;
    int i;

    CHECK_PARAM_RET(adpt, EIO);
    CHECK_PARAM_RET(msgs, EIO);
    CHECK_PARAM_RET(num > 0, EIO);

    dev = (struct CH347_device *)adpt->algo_data;

    CHECK_PARAM_RET(dev, EIO);

    for (i = 0; i < num; ++i) {
        if (msgs[i].flags & I2C_M_TEN) {
            dev_err(&dev->iface->dev, "10 bit i2c addresses are not supported");
            result = -EINVAL;
            break;
        }
        if (msgs[i].flags & I2C_M_RD) {
            result = CH347_i2c_read(dev, &msgs[i]); // checks for NACK of msg->addr
            if (result < 0)
                break;
        } else {
            result = CH347_i2c_write(dev, &msgs[i]);
            if (result < 0)
                break;
        }
    }

    if (result < 0)
        return result;

    return num;
}

static u32 CH347_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm CH347_i2c_algorithm = {
    .master_xfer = CH347_i2c_xfer,
    .functionality = CH347_i2c_func,
};

void CH347_i2c_remove(struct CH347_device *dev)
{
	if (!dev->i2c_init)
		return;

	i2c_del_adapter(&dev->adapter);
}

int CH347_i2c_init(struct CH347_device *dev)
{
	int retval;
	int actual;

	dev->adapter.owner = THIS_MODULE;
	dev->adapter.class = I2C_CLASS_HWMON;
    dev->adapter.algo = &CH347_i2c_algorithm;
	dev->adapter.algo_data = dev;
    snprintf(dev->adapter.name, sizeof(dev->adapter.name), "CH347 I2C USB bus %03d device %03d",
		 dev->usb_dev->bus->busnum, dev->usb_dev->devnum);

	dev->adapter.dev.parent = &dev->iface->dev;

    /* Set CH347 i2c speed */
    mutex_lock(&dev->usb_lock);
    dev->obuf[0] = CH347_CMD_I2C_STREAM;
    dev->obuf[1] = CH347_CMD_I2C_STM_SET | CH347_I2C_STANDARD_SPEED;
    dev->obuf[2] = CH347_CMD_I2C_STM_END;

    retval = usb_bulk_msg(dev->usb_dev, usb_sndbulkpipe(dev->usb_dev, dev->ep_out), dev->obuf, 3, &actual, DEFAULT_TIMEOUT);
    mutex_unlock(&dev->usb_lock);
    if (retval < 0) {
        dev_err(&dev->iface->dev, "Cannot set I2C speed\n");
        return -EIO;
    }

    mutex_lock(&dev->usb_lock);
    memset (dev->obuf, 0, 16);
    if (dev->usb_dev->descriptor.idProduct == 0x55dd) {
        // force GPIO #3 (SCL) to output high
        dev->obuf[0] = 0xcc;
        dev->obuf[1] = 8;
        dev->obuf[3 + 3] = 0xf8;
        retval = usb_bulk_msg(dev->usb_dev, usb_sndbulkpipe(dev->usb_dev, dev->ep_out), dev->obuf, 11, &actual, DEFAULT_TIMEOUT);
    }
    mutex_unlock(&dev->usb_lock);
    if (retval < 0) {
        dev_err(&dev->iface->dev, "Cannot force SCL high\n");
        return -EIO;
    }

	i2c_add_adapter(&dev->adapter);
	dev->i2c_init = true;

	return 0;
}
