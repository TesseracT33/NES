#pragma once

#include "BaseMapper.h"

class UxROM : public BaseMapper
{
public:
	UxROM(const std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, MutateProperties(properties)) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
		}
		// CPU $8000-$BFFF: 16 KiB switchable PRG ROM bank
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
		}
		// CPU $C000-$FFFF: 16 KiB PRG ROM bank, fixed to the last bank
		else
		{
			return prg_rom[addr - 0xC000 + (properties.num_prg_rom_banks - 1) * 0x4000];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = data % properties.num_prg_rom_banks;
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

	u16 TransformNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

protected:
	u8 prg_bank = 0;

private:
	static MapperProperties MutateProperties(MapperProperties properties)
	{
		SetCHRRAMSize(properties, 0x2000);
		return properties;
	};
};