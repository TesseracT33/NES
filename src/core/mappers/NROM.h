#pragma once

#include "BaseMapper.h"

class NROM : public BaseMapper
{
public:
	NROM(const std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, MutateProperties(properties)) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x5FFF)
		{
			return 0xFF;
		}
		// CPU $6000-$7FFF: Family Basic only: PRG RAM, mirrored as necessary to fill entire 8 KiB window, write protectable with an external switch
		if (addr <= 0x7FFF)
		{
			return prg_ram[addr - 0x6000];
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
		if (addr >= 0x6000 && addr <= 0x7FFF)
		{
			prg_ram[addr - 0x6000] = data;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB (not bank switched)
		return chr[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		if (properties.has_chr_ram)
			chr[addr] = data;
	};

private:
	static MapperProperties MutateProperties(MapperProperties properties)
	{
		SetCHRRAMSize(properties, 0x2000); /* Some homebrew programs may use 8 KiB of CHR RAM */
		SetPRGRAMSize(properties, 0x2000);
		return properties;
	};
};

