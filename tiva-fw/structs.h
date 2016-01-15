#pragma once

#include "config.h"
#include "types.h"
#include "checks.h"

#if defined(PIXEL_TYPE_WS2811)
#define EXPANSION_PER_PART 4
#define PARTS_PER_PIXEL		3
typedef struct {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
} bottle;
#elif defined(PIXEL_TYPE_WS2801)
#define PARTS_PER_PIXEL		3
#define EXPANSION_PER_PART 1
typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} bottle;
#else 
#error "no known pixel type was defined"
#endif

typedef struct {
	bottle bottles[CRATE_SIZE];
#if NUM_STATUS_LED > 0
	bottle status[NUM_STATUS_LED];
#endif
} crate;
typedef struct {
#if NUM_BOOTSTRAP_LED > 0
	bottle bootstrap[NUM_BOOTSTRAP_LED];
#endif
	crate crates[CRATES_PER_BUS];
} busbuffer;

typedef struct {
	unsigned char command; /* 0x00 for frame data, 0x01 to initiate latch */
	unsigned char crate_x;
	unsigned char crate_y;
	color_t rgb_data [CRATE_WIDTH][CRATE_HEIGHT];
	//unsigned char rgb_data[CRATE_SIZE*BYTES_PER_PIXEL];
} FramebufferData;
