#pragma once

#include "types.h"

#define CRATE_WIDTH		6
#define CRATE_HEIGHT		4

// seen from the viewer side of the crate
extern const bottlelocation BOTTLE_MAP[CRATE_HEIGHT][CRATE_WIDTH];
#define BOTTLE_MAP_DATA { \
	{22,21,18,17,14,13},\
	{23,20,19,16,15,12},\
	{6,7,8,9,10,11},\
	{5,4,3,2,1,0},\
	}

#define CRATES_X		5
#define CRATES_Y		3
#define BUS_COUNT		3
#define CRATES_PER_BUS		5

extern const layout CRATE_MAP[CRATES_Y][CRATES_X];
#define CRATE_MAP_DATA { \
	{{2,4},{2,3},{2,2},{2,1},{2,0}},\
	{{1,4},{1,3},{1,2},{1,1},{1,0}},\
	{{0,4},{0,3},{0,2},{0,1},{0,0}},\
	 }

#define NUM_BOOTSTRAP_LED 	0
#define NUM_STATUS_LED 		1

#define PIXEL_TYPE_WS2811

#define BUS_ROWS		(CRATES_Y*CRATE_HEIGHT)
#define CRATE_COUNT		(CRATES_X*CRATES_Y)
#define CRATE_SIZE		(CRATE_WIDTH*CRATE_HEIGHT)
#define BUS_SIZE		((NUM_BOOTSTRAP_LED+CRATES_PER_BUS*(CRATE_SIZE+NUM_STATUS_LED))*PARTS_PER_PIXEL*EXPANSION_PER_PART)

#if defined(PIXEL_TYPE_WS2811)
#define SPI_SPEED	3200000
#elif defined(PIXEL_TYPE_WS2801)
#define SPI_SPEED	2000000
#else 
#error "no known pixel type was defined"
#endif
