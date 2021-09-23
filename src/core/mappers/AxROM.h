#pragma once

#include "BaseMapper.h"

class AxROM final : public BaseMapper
{
public:
	AxROM(size_t chr_size, size_t prg_rom_size, size_t prg_ram_size)
		: BaseMapper(chr_size, prg_rom_size, prg_ram_size) {}

	u8 ReadPRG(u16 addr) const override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
		}
		// $8000-$BFFF: 32 KiB switchable PRG ROM bank
		return prg_rom[addr - 0x8000 + prg_bank * 0x8000];
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = (data & 7) % num_prg_rom_banks;
			vram_page = data & 0x10;
		}
	};

	u8 ReadCHR(u16 addr) const override
	{
		// PPU $0000-$1FFF: 8 KiB RAM (not bank switched)
		return chr[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		chr[addr] = data;
	};

	u16 GetNametableAddr(u16 addr) override
	{
		// TODO: not sure about this
		if (vram_page == 0)
			return NametableAddrSingleLower(addr);
		return NametableAddrSingleUpper(addr);
	};

private:
	bool vram_page = 0;
	unsigned prg_bank : 3 = 0;
};