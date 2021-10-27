#pragma once

#include "../System.h"

#include "../../Types.h"

struct MapperProperties
{
	bool hard_wired_four_screen;
	bool has_chr_ram;
	bool has_chr_nvram;
	bool has_prg_ram;
	bool has_prg_nvram;
	bool has_trainer;
	bool mirroring;
	u8 submapper_num;
	u16 mapper_num;
	size_t chr_nvram_size;
	size_t chr_size; /* Either RAM or ROM (a cart cannot have both). */
	size_t prg_nvram_size;
	size_t prg_ram_size;
	size_t prg_rom_size;

	System::VideoStandard video_standard;
};