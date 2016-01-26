#pragma once
#include "structs.h"
#include <stdbool.h>

unsigned long framebuffer_read(void *data, unsigned long len);

extern volatile busbuffer framebuffer1[BUS_COUNT];
extern volatile busbuffer framebuffer2[BUS_COUNT];
extern volatile busbuffer *framebuffer_input;
extern volatile busbuffer *framebuffer_output;

extern bool swap_buffers;
