
#include <linux/usb.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kernel.h>

#include "buf.h"
#include "usb.h"
#include "tty.h"


#define CHIPBEE_MAX_RECV	64

struct chipbee_usb_device {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct mutex io_mutex;

	__u8 ctrl_out_epaddr;
	__u8 bulk_in_epaddr;
	__u8 bulk_out_epaddr;

	struct urb *bulk_in_urb;
	char bulk_in_buffer[CHIPBEE_MAX_RECV];

	wait_queue_head_t read_queue;
	struct chipbee_buf read_buf;

	struct chipbee_tty_device *ttydev;
};



#define CHIPBEE_VENDOR_ID	0x1234
#define CHIPBEE_PRODUCT_ID	0x1234

static const struct usb_device_id chipbee_table[] = {
	{ USB_DEVICE(CHIPBEE_VENDOR_ID, CHIPBEE_PRODUCT_ID) },
	{ },
};
MODULE_DEVICE_TABLE(usb, chipbee_table);


static void chipbee_usb_read_bulk_callback(struct urb *urb)
{
	struct chipbee_usb_device *usbdev = urb->context;

	if (urb->status) {
		dev_err(&usbdev->interface->dev, "error during urb read, %d", urb->status);
		return;
	}

	chipbee_buf_push(&usbdev->read_buf, usbdev->bulk_in_buffer, urb->actual_length);

	usbdev->bulk_in_buffer[urb->actual_length] = '\0';
	printk(KERN_NOTICE "got: %s", usbdev->bulk_in_buffer);

	wake_up_interruptible(&usbdev->read_queue);
}

int chipbee_usb_start_read(struct chipbee_usb_device *usbdev)
{
	int result;

	usb_fill_bulk_urb(usbdev->bulk_in_urb,
			  usbdev->udev,
			  usb_rcvbulkpipe(usbdev->udev, usbdev->bulk_in_epaddr),
			  usbdev->bulk_in_buffer,
			  CHIPBEE_MAX_RECV,
			  chipbee_usb_read_bulk_callback,
			  usbdev);
	//usbdev->bulk_in_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	result = usb_submit_urb(usbdev->bulk_in_urb, GFP_KERNEL);
	if (result) {
		dev_err(&usbdev->interface->dev, "error submitting read urb, %d", result);
		return result;
	}
	return 0;
}

static void chipbee_usb_write_bulk_callback(struct urb *urb)
{
	struct chipbee_usb_device *usbdev;

	usbdev = urb->context;

	usb_free_coherent(urb->dev, urb->transfer_buffer_length, urb->transfer_buffer, urb->transfer_dma);
}

ssize_t chipbee_usb_write_msg(struct chipbee_usb_device *usbdev, const char *message, size_t len)
{
	int result;
	char *buffer = NULL;
	struct urb *urb = NULL;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		return -ENOMEM;

	if (len > 64)
		len = 64;
	//bytes_written = (count > skel->bulk_out_size) ? skel->bulk_out_size : count;

	buffer = usb_alloc_coherent(usbdev->udev, len, GFP_KERNEL, &urb->transfer_dma);
	if (!buffer) {
		result = -ENOMEM;
	   	goto exit;
	}

	memcpy(buffer, message, len);

	usb_fill_bulk_urb(urb,
			  usbdev->udev,
			  usb_sndbulkpipe(usbdev->udev, usbdev->bulk_out_epaddr),
			  buffer,
			  len,
			  chipbee_usb_write_bulk_callback,
			  usbdev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	result = usb_submit_urb(urb, GFP_KERNEL);
	if (result) {
		dev_err(&usbdev->interface->dev, "error submitting write urb, %d", result);
		goto exit;
	}

	result = len;

    exit:
	usb_free_urb(urb);
	return result;
}

static int chipbee_usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int i;
	int result;
	struct chipbee_usb_device *usbdev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;

	printk(KERN_NOTICE "probed for device");

	usbdev = kzalloc(sizeof(struct chipbee_usb_device), GFP_KERNEL);
	if (!usbdev)
		return -ENOMEM;

	// Initialize our interface-specific data
	usbdev->udev = usb_get_dev(interface_to_usbdev(interface));
	usbdev->interface = usb_get_intf(interface);
	init_waitqueue_head(&usbdev->read_queue);
	mutex_init(&usbdev->io_mutex);

	usbdev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbdev->bulk_in_urb) {
		result = -ENOMEM;
		goto fail;
	}


	// Search through the endpoints in the usb descriptor to get the endpoint addresses we'll need
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;
		printk(KERN_NOTICE "found %x %x", endpoint->bEndpointAddress, endpoint->bmAttributes);

		if (!usbdev->bulk_out_epaddr
		  && !(endpoint->bEndpointAddress & USB_DIR_IN)
		  && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
			usbdev->bulk_out_epaddr = endpoint->bEndpointAddress;
		}

		if (!usbdev->bulk_in_epaddr
		  && (endpoint->bEndpointAddress & USB_DIR_IN)
		  && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
			usbdev->bulk_in_epaddr = endpoint->bEndpointAddress;
		}

		if (!usbdev->ctrl_out_epaddr
		  && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_CONTROL)) {
			usbdev->ctrl_out_epaddr = endpoint->bEndpointAddress;
		}
	}

	if (!usbdev->bulk_out_epaddr || !usbdev->bulk_in_epaddr || !usbdev->ctrl_out_epaddr) {
		result = -ENODEV;
		printk(KERN_ALERT "no endpoint found");
		goto fail;
	}


	// Set our interface-specific data in the interface struct so we can retrieve it later
	usb_set_intfdata(interface, usbdev);


	// Create the associated TTY device
	usbdev->ttydev = chipbee_tty_create_device(usbdev, &interface->dev);
	if (!usbdev->ttydev)
		goto fail;


	printk(KERN_NOTICE "chipbee probe success");
	return 0;

    fail:
	printk(KERN_ALERT "error probing chipbee device: %d", result);
	kfree(usbdev);
	return result;
}

static void chipbee_usb_disconnect(struct usb_interface *interface)
{
	struct chipbee_usb_device *usbdev;

	usbdev = usb_get_intfdata(interface);

	// Wake up any processes that are waiting for the read() system call
	wake_up_interruptible(&usbdev->read_queue);

	// Cancel and free any read requests in progress
	usb_kill_urb(usbdev->bulk_in_urb);
	usb_free_urb(usbdev->bulk_in_urb);

	kfree(usbdev);
	usb_set_intfdata(interface, NULL);
	chipbee_tty_release_device(usbdev->ttydev);
}


struct usb_driver chipbee_driver_usb = {
	.name         = "ChipBee",
	.id_table     = chipbee_table,
	.probe        = chipbee_usb_probe,
	.disconnect   = chipbee_usb_disconnect,
};


int chipbee_usb_init()
{
	int result;

        result = usb_register(&chipbee_driver_usb);
        if (result < 0) {
                printk(KERN_NOTICE "usb_register failed for the " __FILE__ " driver. Error number %d", result);
                return -1;
        }
	return 0;
}

void chipbee_usb_exit()
{
        usb_deregister(&chipbee_driver_usb);
}

