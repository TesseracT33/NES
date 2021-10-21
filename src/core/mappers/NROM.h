#pragma once

#include "BaseMapper.h"

class NROM final : public BaseMapper
{
public:
	NROM(MapperProperties mapper_properties) : BaseMapper(mapper_properties) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x5FFF)
		{
			throw std::runtime_error(std::format("Invalid address ${:X} given as argument to NROM::ReadPRG(u16).", addr));
		}
		// CPU $6000-$7FFF: Family Basic only: PRG RAM, mirrored as necessary to fill entire 8 KiB window, write protectable with an external switch
		if (addr <= 0x7FFF)
		{
			if (properties.has_prg_ram)
				return prg_ram[addr - 0x6000];
			return 0xFF;
		}
		// CPU $8000-$BFFF: First 16 KiB of ROM.
		if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000];
		}
		// CPU $C000-$FFFF: Last 16 KiB of ROM (NROM-256) or mirror of $8000-$BFFF (NROM-128).
		return prg_rom[addr - (properties.prg_rom_size == 0x4000 ? 0xC000 : 0x8000)];
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x6000 && addr <= 0x7FFF && properties.has_prg_ram)
		{
			prg_ram[addr - 0x6000] = data;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB (not bank switched)
		return chr[addr];
	}

	void WriteCHR(u16 addr, u8 data) override
	{
		if (properties.has_chr_ram)
			chr[addr] = data;
	}

	u16 GetNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};
};

