
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "tty.h"
#include "usb.h"


MODULE_LICENSE("GPL");


//struct chipbee_data *chipbee_devs;


static int chipbee_init(void)
{
	int result = 0;

	printk(KERN_ALERT "Hello, world\n");

	//chipbee_devs = kmalloc(sizeof(struct chipbee_data) * NUM_DEVICES, GFP_KERNEL);
	//memset(chipbee_devs, 0, sizeof(struct chipbee_data) * NUM_DEVICES);

	//result = chipbee_fops_init();
	//if (result < 0)
	//	return result;

	result = chipbee_tty_init();
	if (result < 0)
		return result;

	result = chipbee_usb_init();
	if (result < 0) {
		chipbee_tty_exit();
		return result;
	}

	return 0;
}

static void chipbee_exit(void)
{
	//chipbee_fops_exit();
	chipbee_usb_exit();
	chipbee_tty_exit();

	//kfree(chipbee_devs);

	printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(chipbee_init);
module_exit(chipbee_exit);

