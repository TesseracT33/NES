#pragma once

#include "BaseMapper.h"

class UxROM final : public BaseMapper
{
public:
	UxROM(size_t chr_size, size_t prg_rom_size, size_t prg_ram_size)
		: BaseMapper(chr_size, prg_rom_size, prg_ram_size) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
		}
		// $8000-$BFFF: 16 KiB switchable PRG ROM bank
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
		}
		// $C000-$FFFF: 16 KiB PRG ROM bank, fixed to the last bank
		else
		{
			return prg_rom[addr - 0xC000 + (num_prg_rom_banks - 1) * 0x4000];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = (data & 0xF) % num_prg_rom_banks;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB (not bank switched)
		return chr[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		if (chr_is_ram)
			chr[addr] = data;
	};

	u16 GetNametableAddr(u16 addr) override
	{
		if (mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

private:
	u8 prg_bank = 0;
};