#pragma once

#include "UxROM.h"

class Mapper094 : public UxROM
{
public:
	Mapper094(MapperProperties mapper_properties) : UxROM(mapper_properties) {}

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = (data >> 2) % num_prg_rom_banks;
		}
	};
};