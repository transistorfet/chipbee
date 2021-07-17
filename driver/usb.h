
#ifndef CHIPBEE_USB_H
#define CHIPBEE_USB_H

struct chipbee_usb_device;

int chipbee_usb_init(void);
void chipbee_usb_exit(void);

ssize_t chipbee_usb_write_msg(struct chipbee_usb_device *usbdev, const char *message, size_t len);
int chipbee_usb_start_read(struct chipbee_usb_device *usbdev);

#endif

