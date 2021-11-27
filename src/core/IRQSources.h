#pragma once

#include "../Types.h"

enum class IRQSource : u8
{
	APU_DMC   = 0x01 << 0,
	APU_FRAME = 0x01 << 1,
	MMC3      = 0x01 << 2,
	MMC5      = 0x01 << 3,
	VRC       = 0x01 << 4,
	FME7      = 0x01 << 5,
	NAMCO163  = 0x01 << 6,
	DF5       = 0x01 << 7
};