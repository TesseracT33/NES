#pragma once

#include "BaseMapper.h"

class CNROM : public BaseMapper
{
public:
	CNROM(MapperProperties properties) : BaseMapper(properties),
		num_chr_banks(properties.chr_size / chr_bank_size) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
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
		if (addr >= 0x8000)
		{
			chr_bank = data % num_chr_banks; // The CHR capacity is at most 32 KiB (four 8 KiB banks). chr_bank is 2 bits.
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB switchable CHR ROM bank.
		return chr[addr + 0x2000 * chr_bank];
	}

	u16 TransformNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

protected:
	static const size_t chr_bank_size = 0x2000;
	const size_t num_chr_banks;

	unsigned chr_bank : 2 = 0;
};

