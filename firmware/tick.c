#include "tick.h"
#include "debug.h"
#include "config.h"
#include "led.h"
#include "proto.h"
#include "spi.h"
#include <stdbool.h>

volatile unsigned long g_ulSysTickCount = 0;
volatile unsigned long last_frame = 0;

void SysTickIntHandler(void) {
	static bool waiting = 0;
	g_ulSysTickCount++;
	if(g_ulSysTickCount - last_frame > 250) {
		DEBUG_PRINT("idle since %d\n", last_frame);
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

