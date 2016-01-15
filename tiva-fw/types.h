#pragma once

typedef uint8_t bottlelocation;

typedef struct {
	int bus:4;
	int crate:4;
} layout;

typedef struct {
	unsigned char red, green, blue;
} color_t;

typedef struct {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
} bottle;
