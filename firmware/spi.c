/* Copyright (c) 2014 jaseg
 * Released under GPLv3
 */

#include <inttypes.h>
#include <stdbool.h>
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
#include "types.h"
#include "config.h"
#include "checks.h"
#include "proto.h"
#include "spi.h"

unsigned char ucControlTable[1024] __attribute__ ((aligned(1024)));

void kickoff_transfers() {
	while(MAP_uDMAChannelIsEnabled(11)
			|| MAP_uDMAChannelIsEnabled(25)
			|| MAP_uDMAChannelIsEnabled(13)
			|| MAP_uDMAChannelIsEnabled(15)){
		UARTprintf("A DMA tranfer is still running\n");
		/* Idle for some time to give the µDMA controller a chance to complete its job */
		SysCtlDelay(5000);
	}
	/* Wait 1.2ms (20kCy @ 50MHz) to ensure the WS2801 latch this frame's data */
	SysCtlDelay(20000);
	/* Swap buffers */
	volatile busbuffer *tmp = framebuffer_output;
	framebuffer_output = framebuffer_input;
	framebuffer_input = tmp;
	/* Re-schedule DMA transfers */
	// caution, du to funny alignments of the buffers, these need to stay in order, to prevent someone from clearing the wrong stuff...
	kickoff_transfer(11, 0, SSI0_BASE);
	//kickoff_transfer(25, 1, SSI1_BASE);
	kickoff_transfer(13, 2, SSI2_BASE);
	kickoff_transfer(15, 1, SSI3_BASE);
}

inline void kickoff_transfer(unsigned int channel, unsigned int offset, int base) {
	MAP_uDMAChannelTransferSet(channel | UDMA_PRI_SELECT, UDMA_MODE_BASIC, (unsigned char*)framebuffer_output+BUS_SIZE*offset, (void *)(base + SSI_O_DR), BUS_SIZE);
	MAP_uDMAChannelEnable(channel);
}

void ssi_udma_channel_config(unsigned int channel) {
	/* Set the USEBURST attribute for the uDMA SSI TX channel.	This will force the controller to always use a burst
	 * when transferring data from the TX buffer to the SSI.  This is somewhat more effecient bus usage than the default
	 * which allows single or burst transfers. */
	MAP_uDMAChannelAttributeEnable(channel, UDMA_ATTR_USEBURST);
	/* Configure the SSI Tx µDMA Channel to transfer from RAM to TX FIFO. The arbitration size is set to 4, which
	 * matches the SSI TX FIFO µDMA trigger threshold. */
	MAP_uDMAChannelControlSet(channel | UDMA_PRI_SELECT, UDMA_SIZE_8 | UDMA_SRC_INC_8 | UDMA_DST_INC_NONE | UDMA_ARB_4);
}

void spi_init(void) {
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_GPIOPinConfigure(GPIO_PA2_SSI0CLK);
	MAP_GPIOPinConfigure(GPIO_PA5_SSI0TX);
	MAP_GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2 | GPIO_PIN_5);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	MAP_GPIOPinConfigure(GPIO_PF2_SSI1CLK);
	MAP_GPIOPinConfigure(GPIO_PF1_SSI1TX);
	MAP_GPIOPinTypeSSI(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_1);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	MAP_GPIOPinConfigure(GPIO_PB4_SSI2CLK);
	MAP_GPIOPinConfigure(GPIO_PB7_SSI2TX);
	MAP_GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_4 | GPIO_PIN_7);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	MAP_GPIOPinConfigure(GPIO_PD0_SSI3CLK);
	MAP_GPIOPinConfigure(GPIO_PD3_SSI3TX);
	MAP_GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_0 | GPIO_PIN_3);


	/* Configure SSI0..3 for the ws2801's SPI-like protocol */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI3);

	MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, SPI_SPEED, 8);
	MAP_SSIConfigSetExpClk(SSI1_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, SPI_SPEED, 8);
	MAP_SSIConfigSetExpClk(SSI2_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, SPI_SPEED, 8);
	MAP_SSIConfigSetExpClk(SSI3_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, SPI_SPEED, 8);

	/* Configure the µDMA controller for use by the SPI interface */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
	MAP_SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UDMA);
	// FIXME what do we need this for? IntEnable(INT_UDMAERR); // Enable µDMA error interrupt
	MAP_uDMAEnable();
	MAP_uDMAControlBaseSet(ucControlTable);
	
	MAP_uDMAChannelAssign(UDMA_CH11_SSI0TX);
	MAP_uDMAChannelAssign(UDMA_CH25_SSI1TX);
	MAP_uDMAChannelAssign(UDMA_CH13_SSI2TX);
	MAP_uDMAChannelAssign(UDMA_CH15_SSI3TX);
	
	ssi_udma_channel_config(11);
	ssi_udma_channel_config(25);
	ssi_udma_channel_config(13);
	ssi_udma_channel_config(15);

	MAP_SSIDMAEnable(SSI0_BASE, SSI_DMA_TX);
	MAP_SSIDMAEnable(SSI1_BASE, SSI_DMA_TX);
	MAP_SSIDMAEnable(SSI2_BASE, SSI_DMA_TX);
	MAP_SSIDMAEnable(SSI3_BASE, SSI_DMA_TX);

	/* Enable the SSIs after configuring anything around them. */
	MAP_SSIEnable(SSI0_BASE);
	MAP_SSIEnable(SSI1_BASE);
	MAP_SSIEnable(SSI2_BASE);
	MAP_SSIEnable(SSI3_BASE);
}
