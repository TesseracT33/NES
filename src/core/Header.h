#pragma once

#include "../Types.h"

// Contains info stored in the header of a rom
struct Header
{
	bool has_prg_ram;
	bool has_trainer;
	bool ignore_mirroring_control;
	bool mirroring;
	size_t chr_size;
	size_t prg_size;
	u8 mapper_num;
};