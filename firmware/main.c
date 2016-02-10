/* Copyright (c) 2014 jaseg
 * Released under GPLv3
 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "types.h"
#include "config.h"
#include "checks.h"
#include "proto.h"
#include "output.h"
#include "log.h"
#include "com.h"
#include "led.h"
#include "tick.h"
#include "platform.h"



//volatile unsigned long g_ulFlags = 0;
//char *g_pcStatus;


int main(void) {
	platform_init();

	debug_init();
	DEBUG_PRINT("Booting...\n\n");

	com_init();
	
	spi_init();

	init_framebuffer(framebuffer1);
	init_framebuffer(framebuffer2);

	DEBUG_PRINT("Booted.\n");

	while(1);
}
