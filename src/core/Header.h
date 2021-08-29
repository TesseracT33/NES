#pragma once

#include "../Types.h"

// contains info stored in the header of a rom
struct Header final
{
	bool has_trainer;
	bool has_prg_ram;
	size_t prg_size;
	size_t chr_size;
	u8 mapper_num;
};