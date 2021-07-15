
#ifndef CHIPBEE_USB_H
#define CHIPBEE_USB_H

struct chipbee_usb_device;

int chipbee_usb_init(void);
void chipbee_usb_exit(void);

ssize_t chipbee_usb_write_buf(struct chipbee_usb_device *dev, const char *buffer, size_t len);
ssize_t chipbee_usb_write_available(struct chipbee_usb_device *usbdev);

#endif

