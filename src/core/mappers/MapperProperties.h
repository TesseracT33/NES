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
	size_t num_chr_banks;
	size_t num_prg_ram_banks;
	size_t num_prg_rom_banks;
	size_t chr_bank_size = 0x2000; /* The default; can be modified by a mapper in its constructor. */
	size_t prg_rom_bank_size = 0x4000;
	size_t prg_ram_bank_size = 0x4000;

	System::VideoStandard video_standard;
};