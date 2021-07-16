
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/tty_flip.h>

#include "usb.h"
#include "tty.h"

#define CHIPBEE_TTY_MAJOR		240
#define CHIPBEE_TTY_MAX_MINORS		2


struct chipbee_tty_device {
	struct tty_struct *tty;
	struct tty_port port;
	int minor;
	struct chipbee_usb_device *usbdev;
};

static struct tty_driver *chipbee_tty_driver;
static struct chipbee_tty_device *chipbee_tty_minors[CHIPBEE_TTY_MAX_MINORS];


static int chipbee_tty_open(struct tty_struct *tty, struct file *fp)
{
	struct chipbee_tty_device *ttydev = chipbee_tty_minors[tty->index];

	tty->driver_data = ttydev;
	ttydev->tty = tty;
	return 0;
}

static void chipbee_tty_close(struct tty_struct *tty, struct file *fp)
{
	//struct chipbee_tty_device *ttydev = tty->driver_data;

	//tty_port_close(&ttydev->port, tty, fp);
}

static int chipbee_tty_write(struct tty_struct *tty, const unsigned char *buffer, int len)
{
	struct chipbee_tty_device *ttydev = tty->driver_data;

	chipbee_usb_write_buf(ttydev->usbdev, buffer, len);
	chipbee_tty_insert(ttydev, buffer, len);
	return len;
}

static int chipbee_tty_write_room(struct tty_struct *tty)
{
	struct chipbee_tty_device *ttydev = tty->driver_data;

	return chipbee_usb_write_available(ttydev->usbdev);
}

int chipbee_tty_insert(struct chipbee_tty_device *ttydev, const char *buffer, int len)
{
	tty_insert_flip_string(&ttydev->port, buffer, len);
	tty_flip_buffer_push(&ttydev->port);
	return len;
}


const struct tty_operations chipbee_tty_ops = {
	.open       = chipbee_tty_open,
	.close      = chipbee_tty_close,
	.write      = chipbee_tty_write,
	.write_room = chipbee_tty_write_room,
};



struct chipbee_tty_device *chipbee_tty_create_device(struct chipbee_usb_device *usbdev, struct device *device)
{
	int minor = 0;
	struct chipbee_tty_device *ttydev;

	// Find an unused minor device
	while (1) {
		// TODO this should probably have locking
		if (minor >= CHIPBEE_TTY_MAX_MINORS) {
			dev_err(device, "no chipbee minor device numbers left");
			return 0;
		}
		if (!chipbee_tty_minors[minor])
			break;
		minor++;
	}
	dev_info(device, "registering chipbee tty device %d", minor);


	// Allocate tty driver specific data and set the minor device pointer so we can find it when the device is opened
	ttydev = kzalloc(sizeof(struct chipbee_tty_device), GFP_KERNEL);
	if (!ttydev)
		return NULL;
	chipbee_tty_minors[minor] = ttydev;

	ttydev->usbdev = usbdev;
	ttydev->minor = minor;
	tty_port_init(&ttydev->port);

	// Register the new device with the TTY subsystem
	tty_port_register_device(&ttydev->port, chipbee_tty_driver, minor, device);

	return ttydev;
}

void chipbee_tty_release_device(struct chipbee_tty_device *ttydev)
{
	tty_unregister_device(chipbee_tty_driver, ttydev->minor);
	chipbee_tty_minors[ttydev->minor] = NULL;
	kfree(ttydev);
}

int chipbee_tty_init(void)
{
	int result;

	memset(chipbee_tty_minors, 0, sizeof(struct chipbee_tty_device *) * CHIPBEE_TTY_MAX_MINORS);

	chipbee_tty_driver = alloc_tty_driver(CHIPBEE_TTY_MAX_MINORS);
	if (!chipbee_tty_driver)
		return -ENOMEM;

	chipbee_tty_driver->owner = THIS_MODULE;
	chipbee_tty_driver->driver_name = "chipbeetty";
	chipbee_tty_driver->name = "chipbeetty";
	chipbee_tty_driver->major = CHIPBEE_TTY_MAJOR,
	chipbee_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	chipbee_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	chipbee_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	chipbee_tty_driver->init_termios = tty_std_termios;
	chipbee_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(chipbee_tty_driver, &chipbee_tty_ops);

	// Only register the driver here.  TTY devices will be registered as USB devices are connected
	result = tty_register_driver(chipbee_tty_driver);
	if (result) {
                printk(KERN_ALERT "tty_register_driver failed for " __FILE__ ": %d", result);
		put_tty_driver(chipbee_tty_driver);
		return result;
	}

	return 0;
}

void chipbee_tty_exit(void)
{
	tty_unregister_driver(chipbee_tty_driver);
}

