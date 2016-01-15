#pragma once

#include "config.h"
#include "structs.h"


_Static_assert( PARTS_PER_PIXEL * EXPANSION_PER_PART == sizeof(bottle), "bottle size does not match");
_Static_assert( BUS_SIZE == sizeof(busbuffer), "bus buffer size differs from BUS_SIZE");
