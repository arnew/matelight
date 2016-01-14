#ifndef __USB_H__
#define __USB_H__

#include <libusb-1.0/libusb.h>
#include "config.h"

typedef struct {
	libusb_device_handle *handle;
    char *serial;
} matelight_handle;

int matelight_usb_init(void);
void matelight_usb_destroy(void);
matelight_handle *matelight_open(char *match_serial);
int matelight_send_frame(matelight_handle *ml, void *buf, size_t w, size_t h, float brightness, int alpha);

#endif//__USB_H__
