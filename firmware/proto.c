/* Copyright (c) 2014 jaseg
 * Released under GPLv3
 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "types.h"
#include "config.h"
#include "checks.h"
#include "log.h"
#include "proto.h"
#include "output.h"
#include "tick.h"
#include "led.h"

volatile busbuffer framebuffer1[BUS_COUNT];
volatile busbuffer framebuffer2[BUS_COUNT];
volatile busbuffer *framebuffer_input = framebuffer1;
volatile busbuffer *framebuffer_output = framebuffer2;

volatile unsigned long g_ulFlags = 0;
char *g_pcStatus;


FramebufferData accu;
unsigned long fill;
bool toggle = 1;
bool swap_buffers = 0;
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
		swap_buffers = 1;
		kickoff_transfers();
		last_frame = g_ulSysTickCount;
		col_toggle = !col_toggle;
	}else{
		if(len != sizeof(FramebufferData))
			//UARTprintf("got %d, expected %d\n",len, sizeof(FramebufferData));
			goto length_error;

complete_framebuffer:
		if(fb->crate_x > CRATES_X || fb->crate_y > CRATES_Y){
			DEBUG_PRINT("Invalid frame index\n");
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
		set_status_leds(framebuffer_input, bus, crate, col_toggle?black:green);
		set_bootstrap_leds(framebuffer_input, bus, col_toggle?black:green);
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
	DEBUG_PRINT("Invalid packet length\n");
	return len;
}

