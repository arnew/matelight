/* Copyright (c) 2014 jaseg
 * Released under GPLv3
 */

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_ssi.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/udma.h"
#include "driverlib/ssi.h"
#include "usblib/usblib.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdbulk.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "usb_bulk_structs.h"
#include <string.h>
#include "log.h"
#include "proto.h"

static volatile tBoolean g_bUSBConfigured = false;

unsigned long usb_rx_handler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue, void *pvMsgData) {
	unsigned int read;
	switch(ulEvent) {
		case USB_EVENT_CONNECTED:
			g_bUSBConfigured = true;
			DEBUG_PRINT("Host connected.\n");
			break;
		case USB_EVENT_DISCONNECTED:
			g_bUSBConfigured = false;
			DEBUG_PRINT("Host disconnected.\n");
			break;
		case USB_EVENT_RX_AVAILABLE:
			DEBUG_PRINT("Handling host data.\n");
			/* Beware of the cast, it might bite. */
			read = USBDBulkPacketRead((void *)&g_sBulkDevice, usb_rx_buffer, BULK_BUFFER_SIZE, 1);
			return framebuffer_read(usb_rx_buffer, read);
		case USB_EVENT_SUSPEND:
		case USB_EVENT_RESUME:
			break;
		default:
			break;
	}
	return 0;
}

void com_init(void) {
	g_bUSBConfigured = false;

	/* Enable the GPIO peripheral used for USB, and configure the USB pins. */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	MAP_GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	/* Configure USB */
	USBStackModeSet(0, USB_MODE_FORCE_DEVICE, 0);
	USBDBulkInit(0, (tUSBDBulkDevice *)&g_sBulkDevice);
}
