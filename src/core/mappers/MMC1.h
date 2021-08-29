#pragma once

#include "BaseMapper.h"

class MMC1 final : public BaseMapper
{
public:
	u8 ReadPRG(u16 addr) const override
	{
		if (addr <= 0x5FFF)
		{
			return 0xFF;
		}
		else if (addr <= 0x7FFF)
		{
			return prg_ram[addr - 0x6000];
		}
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000];
		}
		else
		{
			return 0xFF;
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr <= 0x7FFF)
		{
			prg_ram[addr - 0x6000] = data;
		}
	};

	u8 ReadCHR(u16 addr) const override
	{
		return u8();
	};

	void WriteCHR(u16 addr, u8 data) override
	{

	};

private:
	u8 current_PRG_ROM_bank = 0;
};