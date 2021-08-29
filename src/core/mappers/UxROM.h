#pragma once

#include "BaseMapper.h"

class UxROM final : public BaseMapper
{
public:
	u8 ReadPRG(u16 addr) const override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
		}
		// $8000-$BFFF: 16 KB switchable PRG ROM bank
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000 + prg_rom_bank * prg_piece_size];
		}
		// $C000-$FFFF: 16 KB PRG ROM bank, fixed to the last bank
		else
		{
			return prg_rom[addr - 0x8000 + (GetNumPRGRomBanks() - 1) * prg_piece_size];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000 && addr <= 0xBFFF)
		{
			prg_rom_bank = data % GetNumPRGRomBanks();
		}
	};

	u8 ReadCHR(u16 addr) const override
	{
		return chr_rom[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{

	};

private:
	u8 prg_rom_bank = 0;
};