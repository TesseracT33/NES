#pragma once

#include "BaseMapper.h"

class CNROM final : public BaseMapper
{
public:
	CNROM(MapperProperties mapper_properties) : BaseMapper(mapper_properties) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			throw std::runtime_error(std::format("Invalid address ${:X} given as argument to CNROM::ReadPRG(u16).", addr));
		}
		// CPU $8000-$BFFF: First 16 KiB of ROM.
		if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000];
		}
		// CPU $C000-$FFFF: Last 16 KiB of ROM (CNROM-256) or mirror of $8000-$BFFF (CNROM-128).
		return prg_rom[addr - (properties.prg_rom_size == 0x4000 ? 0xC000 : 0x8000)];
	};

	void WritePRG(u16 addr, u8 data) override
	{

	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB switchable CHR ROM bank (up to four banks in total).
		return chr[addr + 0x2000 * chr_bank];
	}

	void WriteCHR(u16 addr, u8 data) override
	{
		chr_bank = data & (properties.num_chr_banks - 1);
	}

	u16 GetNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

private:
	unsigned chr_bank : 2 = 0;
};

