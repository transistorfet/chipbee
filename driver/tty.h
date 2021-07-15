
#ifndef CHIPBEE_TTY_H
#define CHIPBEE_TTY_H

#include "usb.h"

struct chipbee_tty_device;

int chipbee_tty_init(void);
void chipbee_tty_exit(void);

struct chipbee_tty_device *chipbee_tty_create_device(struct chipbee_usb_device *usbdev, struct device *device);
void chipbee_tty_release_device(struct chipbee_tty_device *tty);
int chipbee_tty_insert(struct chipbee_tty_device *ttydev, const char *buffer, int len);

#endif

