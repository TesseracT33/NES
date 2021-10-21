#pragma once

#include "BaseMapper.h"

class UxROM final : public BaseMapper
{
public:
	UxROM(MapperProperties mapper_properties) : BaseMapper(mapper_properties) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			throw std::runtime_error(std::format("Invalid address ${:X} given as argument to UxROM::ReadPRG(u16).", addr));
		}
		// $8000-$BFFF: 16 KiB switchable PRG ROM bank
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
		}
		// $C000-$FFFF: 16 KiB PRG ROM bank, fixed to the last bank
		else
		{
			return prg_rom[addr - 0xC000 + (properties.num_prg_rom_banks - 1) * 0x4000];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = (data & 0xF) % properties.num_prg_rom_banks;
		}
		throw std::runtime_error(std::format("Invalid address ${:X} given as argument to UxROM::WritePRG(u16, u8).", addr));
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

	u16 GetNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

private:
	u8 prg_bank = 0;
};