#pragma once
#include "structs.h"

void init_framebuffer(volatile busbuffer * buf);
void set_status_leds(volatile busbuffer* buf, unsigned int bus, unsigned int crate, const color_t c);
void set_bootstrap_leds(volatile busbuffer* buf, unsigned int bus, const color_t c);
void set_bottle(volatile busbuffer* buf, unsigned int bus, unsigned int crate, int x, int y, const color_t c);

extern const color_t white;
extern const color_t black;
extern const color_t red;
extern const color_t green;
