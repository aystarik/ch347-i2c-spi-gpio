WinChipHead (沁恒) CH347 linux driver for I2C / SPI and GPIO mode
=================================================================

They work in 4 different modes, with only two being presented
depending on the USB PID::

  - 0x55db: SPI/I2C/GPIO mode, covered by this ch347_buses driver
  - 0x55dd: JTAG/I2C/GPIO mode, covered by this ch347_buses driver



Building the driver
-------------------

The driver will build for the active kernel::

  $ make

This will create `ch347-buses.ko`, which can the be insmod'ed.

The driver has been tested with a linux kernel 5.19

Setup
-----

Although it's possible to access everything as root, or even to give a
user some rights to access the i2c/spi/gpio subsystem, some of these
resources are critical to the system, and reading or writing to them
might make a system unstable.

  WARNING! Accidentally accessing a motherboard or graphic card I2C or
  SPI device may render the former unoperable. Double check that the
  correct device is accessed.

The following is more safe. As root, create a group, add the user to
the group and create a udev rule for that group that will bind to the
devices recognized by the driver::

  $ groupadd ch347
  $ adduser "$USER" ch347
  $ echo 'SUBSYSTEMS=="usb" ATTRS{idProduct}=="55db" ATTRS{idVendor}=="1a86" GROUP="ch347" MODE="0660"' > /etc/udev/rules.d/99-ch347.rules
  $ echo 'SUBSYSTEMS=="usb" ATTRS{idProduct}=="55dd" ATTRS{idVendor}=="1a86" GROUP="ch347" MODE="0660"' >> /etc/udev/rules.d/99-ch347.rules

After plugging in the USB device, the various /dev entries will be
accessible to the ch341 group::

  $ ls -l /dev/* | grep ch341
  crw-rw----   1 root ch341   254,   2 Sep 20 01:12 /dev/gpiochip2
  crw-rw----   1 root ch341    89,  11 Sep 20 01:12 /dev/i2c-11
  crw-rw----   1 root ch341   153,   0 Sep 20 01:12 /dev/spidev0.0


I2C
---

The ch347 supports 4 different speeds: 20kHz, 100kHz, 400kHz and
750kHz. The driver only supports 100kHz by default, and that currently
cannot be dynamically changed. It is possible to change it in the
ch347_i2c_init() function. A future patch should address that issue.

To find the device number::

  $ i2cdetect -l
  ...
  i2c-11        unknown           CH347 I2C USB bus 003 device 005        N/A

Adding support for a device supported by Linux is easy. For instance::

  modprobe bmi160_i2c
  echo "bmi160 0x68" > /sys/bus/i2c/devices/i2c-$DEV/new_device

or::

  modprobe tcs3472
  echo "tcs3472 0x29" > /sys/bus/i2c/devices/i2c-$DEV/new_device

Files from these drivers will be created somewhere in
/sys/bus/i2c/devices/i2c-$DEV/

Caveats
~~~~~~~

The ch341 doesn't work with a Wii nunchuk, possibly because the
pull-up value is too low (1500 ohms).

i2c AT24 eeproms can be read but not programmed properly because the
at24 linux driver tries to write a byte at a time, and doesn't wait at
all (or enough) between writes. Data corruption on writes does occur.

The driver doesn't support detection of I2C device present on the
bus. Apparently when a device is not present at a given address, the
CH341 will return an extra byte of data, but the driver doesn't
support that. This may be addressed in a future patch.


The GPIOs
---------

16 GPIOs are available on the CH341 A/B/F. The first 6 are input/output,
and the last 10 are input only.

Pinout and their names as they appear on some breakout boards::

  CH341A/B/F     GPIO  Names                    Mode
    pin          line

   15             0     D0, CS0                  input/output
   16             1     D1, CS1                  input/output
   17             2     D2, CS2                  input/output
   18             3     D3, SCK, DCK             input/output
   19             4     D4, DOUT2, CS3           input/output
   20             5     D5, MOSI, DOUT, SDO      input/output
   21             6     D6, DIN2                 input
   22             7     D7, MISO, DIN            input
    5             8     ERR                      input
    6             9     PEMP                     input
    7            10     INT                      input
    8            11     SLCT (SELECT)            input
    ?            12     ?                        input
   27            13     WT (WAIT)                input
    4            14     DS (Data Select?)        input
    3            15     AS (Address Select?)     input


They can be used with the standard linux GPIO interface. Note that
MOSI/MISO/SCK may be used by SPI, when SPI is enabled.

To drive the GPIOs, one can use the regular linux tools. `gpiodetect`
will report the device number to use for the other tools (run as root)::

  $ gpiodetect
  ...
  gpiochip2 [ch341] (16 lines)

  $ gpioinfo gpiochip2
  gpiochip2 - 16 lines:
          line   0:      unnamed       unused   input  active-high
          line   1:      unnamed       unused   input  active-high
          line   2:      unnamed       unused   input  active-high
          line   3:      unnamed       unused   input  active-high
          line   4:      unnamed       unused   input  active-high
          line   5:      unnamed       unused   input  active-high
          line   6:      unnamed       unused   input  active-high
          line   7:      unnamed       unused   input  active-high
	  [......]
          line  15:      unnamed       unused   input  active-high

  $ gpioset gpiochip2 0=0 1=1 2=0
  $ gpioget gpiochip2 5

If the SPI mode is enabled, the MOSI, MISO and SCK, and possible one
or more of CS0/1/2, won't be available.

On Ubuntu 21.04, the `libgpio` is too old and will return an error
when accessing the device. Use a more recent library. The `master`
branch from the git tree works well::

  https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git

GPIO interrupt
~~~~~~~~~~~~~~

The INT pin, corresponding to GPIO 10 is an input pin that can trigger
an interrupt on a rising edge. Only that pin is able to generate an
interrupt, and only on a rising edge. Trying to monitor events on
another GPIO, or that GPIO on something other than a rising edge, will
be rejected.

As an example, physically connect the INT pin to CS2. Start the
monitoring of the INT pin::

  $ gpiomon -r gpiochip2 10

The INT will be triggered by setting CS2 low then high::

  $ gpioset gpiochip2 2=0 && gpioset gpiochip2 2=1

`gpiomon` will report rising events like this:

  event:  RISING EDGE offset: 10 timestamp: [     191.539358302]
  ...


SPI
---

See above for how SPI and GPIO exclusively share some pins.

Only SPI mode 0 (CPOL=0, CPHA=0) appears to be supported by the ch341.

As long as no SPI device has been instantiated, all the GPIOs are
available for general use. When the first device is instantiated, the
driver will try to claim the SPI lines, plus one of the chip select.

To instantiate a device, echo a command string to the device's sysfs
'new_device' file. The command is the driver to use followed by the CS
number. For instance, the following declares a flash memory at CS 0, and a
user device (spidev) at CS 1::

  $ echo "spidev 0" > /sys/class/spi_master/spi0/new_device
  $ echo "spi-nor 1" > /sys/class/spi_master/spi0/new_device

Starting with the Linux kernel 5.15 or 5.16, the following steps are
also needed for each added device for the /dev/spidevX entries to
appear::

    echo spidev > /sys/bus/spi/devices/spi0.0/driver_override
    echo spi0.0 > /sys/bus/spi/drivers/spidev/bind

Change spi0 and spi0.0 as appropriately.

After these command, the GPIO lines will report::

  $ gpioinfo gpiochip2
  gpiochip2 - 16 lines:
          line   0:      unnamed        "CS0"  output  active-high [used]
          line   1:      unnamed        "CS1"  output  active-high [used]
          line   2:      unnamed       unused   input  active-high
          line   3:      unnamed        "SCK"  output  active-high [used]
          line   4:      unnamed       unused   input  active-high
          line   5:      unnamed       "MOSI"  output  active-high [used]
          line   6:      unnamed       unused   input  active-high
          line   7:      unnamed       "MISO"   input  active-high [used]
          line   8:      unnamed       unused   input  active-high
          ...
          line  15:      unnamed       unused   input  active-high

To remove a device, echo its CS to 'delete_device'. The following will
remove the spidev device created on CS 1 above::

  $ echo "1" > /sys/class/spi_master/spi0/delete_device

If all the devices are deleted, the SPI driver will release the SPI
lines, which become available again for GPIO operations.
