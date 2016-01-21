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

unsigned long framebuffer_read(void *data, unsigned long len);
/* Kick off DMA transfer from RAM to SPI interfaces */
void kickoff_transfers(void);
void kickoff_transfer(unsigned int channel, unsigned int offset, int base);
void ssi_udma_channel_config(unsigned int channel);

#define SYSTICKS_PER_SECOND		100
#define SYSTICK_PERIOD_MS		(1000 / SYSTICKS_PER_SECOND)

unsigned char ucControlTable[1024] __attribute__ ((aligned(1024)));

volatile unsigned long g_ulSysTickCount = 0;
volatile unsigned long last_frame = 0;

volatile busbuffer framebuffer1[BUS_COUNT];
volatile busbuffer framebuffer2[BUS_COUNT];
volatile busbuffer *framebuffer_input = framebuffer1;
volatile busbuffer *framebuffer_output = framebuffer2;
const color_t white = {0xff,0xff,0xff};
const color_t black= {0x00,0x00,0x00};
const color_t red= {0xff,0x00,0x00};
const color_t green = {0x00,0xff,0x00};

volatile unsigned long g_ulFlags = 0;
char *g_pcStatus;
static volatile bool g_bUSBConfigured = false;

#ifdef DEBUG
#define DEBUG_PRINT UARTprintf
#else
#define DEBUG_PRINT while(0) ((int (*)(char *, ...))0)
#endif


#if defined(PIXEL_TYPE_WS2811)
uint32_t make_ws2811_bits(uint8_t d) {
	const union { struct { unsigned int a:2, b:2, c:2, d:2; } b; uint8_t i; } data = {.i = d};
	typedef union { struct { unsigned int a:8, b:8, c:8, d:8; } b; uint32_t i; } output8;
	const static uint8_t lookup[4] = {0x88,0x8E,0xE8,0xEE};
	return (output8){.b = {.a = lookup[data.b.a], .b = lookup[data.b.b], .c = lookup[data.b.c], .d = lookup[data.b.d]}}.i;
}
bottle get_pixel(const color_t c) {
	return (bottle) {.red = make_ws2811_bits(c.red), .green = make_ws2811_bits(c.green), .blue = make_ws2811_bits(c.blue)};
}
void init_framebuffer(volatile busbuffer * buf) {
	memset((void*)buf,0x88,BUS_COUNT*BUS_SIZE);
}
#elif defined(PIXEL_TYPE_WS2801)
bottle get_pixel(const color_t c) {
	return (bottle) {.red = c.red, .green = c.green, .blue = c.blue};
}
void init_framebuffer(volatile busbuffer * buf) {}
#else 
#error "no known pixel type was defined"
#endif

void set_bottle(volatile busbuffer* buf, unsigned int bus, unsigned int crate, int x, int y, const color_t c) {
	buf[bus].crates[crate].bottles[BOTTLE_MAP[y][x]] = get_pixel(c);
}
void set_status_leds(volatile busbuffer* buf, unsigned int bus, unsigned int crate, const color_t c) {
#if NUM_STATUS_LED > 0
	for(uint8_t i=0; i< NUM_STATUS_LED; i++) {
		buf[bus].crates[crate].status[i] = get_pixel(c);
	}
#endif
}
void set_bootstrap_leds(volatile busbuffer* buf, unsigned int bus, const color_t c) {
#if NUM_BOOTSTRAP_LED > 0
	for(uint8_t i=0; i< NUM_BOOTSTRAP_LED; i++) {
		buf[bus].bootstrap[i] = get_pixel(c);
	}
#endif
}

void SysTickIntHandler(void) {
	static bool waiting = 0;
	g_ulSysTickCount++;
	if(g_ulSysTickCount - last_frame > 250) {
		UARTprintf("idle since %d\n", last_frame);
		last_frame = g_ulSysTickCount;
		waiting = !waiting;
		if(NUM_STATUS_LED>0) {
			init_framebuffer(framebuffer_input);
			for(unsigned int bus = 0; bus < BUS_COUNT; bus++) {
				for(unsigned int crate = 0; crate < CRATES_PER_BUS; crate++) {
					//for(unsigned int bottle = 0; bottle <= CRATE_SIZE; bottle++) {
					//set_bottle(framebuffer_input, bus, crate, bottle , waiting?white:red);
					//}
					set_status_leds(framebuffer_input, bus, crate, waiting?white:red);
				}
			}
			kickoff_transfers();
		}
	}
}

unsigned long RxHandler(void *pvCBData, unsigned long ulEvent, unsigned long ulMsgValue, void *pvMsgData) {
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
			read = USBDBulkPacketRead((void *)&g_sBulkDevice, g_pui8USBRxBuffer, BULK_BUFFER_SIZE, 1);
			return framebuffer_read(g_pui8USBRxBuffer, read);
		case USB_EVENT_SUSPEND:
		case USB_EVENT_RESUME:
			break;
		default:
			break;
	}
	return 0;
}



FramebufferData accu;
unsigned long fill;
bool toggle = 1;
unsigned long framebuffer_read(void *data, unsigned long len) {
	static bool col_toggle = 0;
	if(len < 1)
		goto length_error;
	DEBUG_PRINT("Rearranging data.\n");
	FramebufferData *fb = (FramebufferData *)data;
	if(fb->command == 1){
		if(len != 1)
			goto length_error;
		DEBUG_PRINT("Starting DMA.\n");
		fill = 0; toggle = 1;
		kickoff_transfers();
		last_frame = g_ulSysTickCount;
		col_toggle = !col_toggle;
	}else{
		if(len != sizeof(FramebufferData))
			//UARTprintf("got %d, expected %d\n",len, sizeof(FramebufferData));
			goto length_error;

complete_framebuffer:
		if(fb->crate_x > CRATES_X || fb->crate_y > CRATES_Y){
			UARTprintf("Invalid frame index\n");
			return len;
		}

		//UARTprintf("crate %d,%d\n",fb->crate_x, fb->crate_y);

		const layout idx = CRATE_MAP[fb->crate_y][fb->crate_x];
		const unsigned int bus = idx.bus;
		const unsigned int crate = idx.crate;
		for(unsigned int x=0; x<CRATE_WIDTH; x++){
			for(unsigned int y=0; y<CRATE_HEIGHT; y++){
				set_bottle(framebuffer_input, bus, crate, x,y,fb->rgb_data[y][x]);
			}
		}
		set_status_leds(framebuffer_input, bus, crate, col_toggle?white:green);
		set_bootstrap_leds(framebuffer_input, bus, col_toggle?white:green);
	}
	return len;
length_error:
	if(len > 1 && len < sizeof(FramebufferData)) {
		//UARTprintf("attempting to fix frame\n");
		fb = (FramebufferData *)data;
		if(toggle && fb->command == 0x00) {
			//UARTprintf("part 1\n");
			memcpy(&accu, data, len);
			fill = len;
			toggle = !toggle;
			return len;
		} else {
			//UARTprintf("potential part 2 fill: %d, len: %d, buffer: %d\n",fill, len, sizeof(FramebufferData));
			if(fill + len == sizeof(FramebufferData)) {
				//UARTprintf("part 2\n");
				memcpy(((char*)&accu) + fill, data, len);
				len += fill;
				fb = &accu;
				toggle = !toggle;
				goto complete_framebuffer;
			}
		}
	}
	fill = 0; toggle = 1;
	UARTprintf("Invalid packet length\n");
	return len;
}


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

//*****************************************************************************
//
// Configure the UART and its pins.  This must be called before UARTprintf().
//
//*****************************************************************************
void
ConfigureUART(void)
{
    //
    // Enable the GPIO Peripheral used by the UART.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Enable UART0
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Configure GPIO Pins for UART mode.
    //
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}

int main(void) {
	/* Enable lazy stacking for interrupt handlers.  This allows floating-point instructions to be used within interrupt
	 * handlers, but at the expense of extra stack usage. */
	MAP_FPULazyStackingEnable();

	/* Set clock to PLL at 50MHz */
	MAP_SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
					   SYSCTL_XTAL_16MHZ);

	/* Configure UART0 pins */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
	MAP_GPIOPinConfigure(GPIO_PA0_U0RX);
	MAP_GPIOPinConfigure(GPIO_PA1_U0TX);
	MAP_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	/* Enable the GPIO pins for the LED (PF2 & PF3). */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	MAP_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_3|GPIO_PIN_2);

	ConfigureUART();
	UARTprintf("Booting...\n\n");

	g_bUSBConfigured = false;

	/* Enable the GPIO peripheral used for USB, and configure the USB pins. */
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	MAP_GPIOPinTypeUSBAnalog(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	/* Enable the system tick. FIXME do we need this? */
	MAP_SysTickPeriodSet(MAP_SysCtlClockGet() / SYSTICKS_PER_SECOND);
	MAP_SysTickIntEnable();
	MAP_SysTickEnable();

	/* Configure USB */
	USBStackModeSet(0, eUSBModeForceDevice, 0);
	USBDBulkInit(0, (tUSBDBulkDevice *)&g_sBulkDevice);

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

#if defined(PIXEL_TYPE_WS2811)
	MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 3200000, 8);
	MAP_SSIConfigSetExpClk(SSI1_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 3200000, 8);
	MAP_SSIConfigSetExpClk(SSI2_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 3200000, 8);
	MAP_SSIConfigSetExpClk(SSI3_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 3200000, 8);
#elif defined(PIXEL_TYPE_WS2801)
	MAP_SSIConfigSetExpClk(SSI0_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 2000000, 8);
	MAP_SSIConfigSetExpClk(SSI1_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 2000000, 8);
	MAP_SSIConfigSetExpClk(SSI2_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 2000000, 8);
	MAP_SSIConfigSetExpClk(SSI3_BASE, MAP_SysCtlClockGet(), SSI_FRF_MOTO_MODE_0, SSI_MODE_MASTER, 2000000, 8);
#else 
#error "no known pixel type was defined"
#endif

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

	UARTprintf("Booted.\n");

	while(1);
}
