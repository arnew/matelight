#pragma once

#include "config.h"
#include "structs.h"


_Static_assert( BYTES_PER_PIXEL * BITS_PER_PIXEL == sizeof(bottle), "bottle size does not match");
_Static_assert( BUS_SIZE == sizeof(busbuffer), "bus buffer size differs from BUS_SIZE");
