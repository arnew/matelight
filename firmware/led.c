/* Copyright (c) 2014 jaseg
 * Released under GPLv3
 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "types.h"
#include "config.h"
#include "checks.h"

const color_t white = {0xff,0xff,0xff};
const color_t black= {0x00,0x00,0x00};
const color_t red= {0xff,0x00,0x00};
const color_t green = {0x00,0xff,0x00};

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

