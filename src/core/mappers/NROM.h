#pragma once

#include "BaseMapper.h"

class NROM final : public BaseMapper
{
public:
	static const size_t prg_ram_size      = 0x2000;
	static const size_t prg_rom_size_base = 0x4000;
	static const size_t chr_rom_size      = 0x2000;

	enum class Variant { PRG_128, PRG_256 } variant;

	void Initialize() override
	{
		this->variant = this->prg_rom.size() == prg_rom_size_base ? Variant::PRG_128 : Variant::PRG_256;
		prg_ram.resize(prg_ram_size);
	}

	u8 ReadPRG(u16 addr) const override
	{
		if (addr <= 0x5FFF)
		{
			return 0xFF;
		}
		else if (addr <= 0x7FFF)
		{
			return prg_ram[addr - 0x6000];
		}
		else if (addr <= 0xBFFF)
		{
			return prg_rom[addr - 0x8000];
		}
		else
		{
			return prg_rom[addr - (variant == Variant::PRG_128 ? 0xC000 : 0x8000)];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr <= 0x7FFF)
		{
			prg_ram[addr - 0x6000] = data;
		}
	};

	u8 ReadCHR(u16 addr) const override
	{
		return chr_rom[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{

	};

	u16 GetNametableAddr(u16 addr) override
	{
		// Horizontal mirroring; addresses in $2400-$27FF and $2C00-$2FFF are transformed into $2000-$23FF and $2800-$2BFF, respectively.
		if (mirroring == 0)
			return addr & ~0x400;
		// Vertical mirroring; addresses in $2800-$2FFF are transformed into $2000-$27FF.
		return addr & ~0x800;
	};
};

