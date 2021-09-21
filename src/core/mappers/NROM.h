#pragma once

#include "BaseMapper.h"

class NROM final : public BaseMapper
{
public:
	NROM(size_t chr_size, size_t prg_rom_size, size_t prg_ram_size)
		: BaseMapper(chr_size, prg_rom_size, prg_ram_size) {}

	u8 ReadPRG(u16 addr) const override
	{
		if (addr <= 0x5FFF)
		{
			return 0xFF;
		}
		// CPU $6000-$7FFF: Family Basic only: PRG RAM, mirrored as necessary to fill entire 8 KiB window, write protectable with an external switch
		if (addr <= 0x7FFF)
		{
			if (has_prg_ram)
				return prg_ram[addr - 0x6000];
			return 0xFF;
		}
		// CPU $8000-$BFFF: First 16 KiB of ROM.
		if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000];
		}
		// CPU $C000-$FFFF: Last 16 KiB of ROM (NROM-256) or mirror of $8000-$BFFF (NROM-128).
		return prg_rom[addr - (prg_rom_size == prg_rom_bank_size ? 0xC000 : 0x8000)];
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x6000 && addr <= 0x7FFF)
		{
			prg_ram[addr - 0x6000] = data;
		}
	};

	u8 ReadCHR(u16 addr) const override
	{
		// PPU $0000-$1FFF: 8 KiB (not bank switched)
		return chr[addr];
	}

	void WriteCHR(u16 addr, u8 data) override
	{
		if (chr_is_ram)
			chr[addr] = data;
	}

	u16 GetNametableAddr(u16 addr) override
	{
		if (mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};
};

