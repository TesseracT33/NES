#pragma once

#include "mappers/MapperEnum.h"

// contains info stored in the header of a rom
struct Header final
{
	bool has_trainer;
	size_t prg_size;
	size_t chr_size;
	MapperNum mapper_num;
};