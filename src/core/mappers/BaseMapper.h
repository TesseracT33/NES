#pragma once

#include <vector>

#include "../Header.h"

#include "../../Types.h"

class BaseMapper
{
public:
	BaseMapper(size_t _chr_size, size_t _prg_rom_size, size_t _prg_ram_size) :
		chr_size(_chr_size),
		prg_rom_size(_prg_rom_size),
		prg_ram_size(_prg_ram_size),
		num_chr_banks(_chr_size / chr_bank_size),
		num_prg_rom_banks(prg_rom_size / prg_rom_bank_size),
		num_prg_ram_banks(prg_ram_size / prg_ram_bank_size)
	{
		chr.resize(chr_size);
		prg_rom.resize(prg_rom_size);
		prg_ram.resize(prg_ram_size);
	}

	static const size_t chr_bank_size = 0x2000;
	static const size_t prg_ram_bank_size = 0x2000;
	static const size_t prg_rom_bank_size = 0x4000;

	void LayoutMemory(u8* rom_arr)
	{
		memcpy(&prg_rom[0], rom_arr, prg_rom_size);
		memcpy(&chr[0], rom_arr + prg_rom_size, chr_size);
	}

	virtual u8   ReadPRG(u16 addr) = 0;
	virtual void WritePRG(u16 addr, u8 data) = 0;

	virtual u8   ReadCHR(u16 addr) = 0;
	virtual void WriteCHR(u16 addr, u8 data) = 0;

	virtual u16 GetNametableAddr(u16 addr) = 0;

protected:
	friend class Cartridge;

	const size_t chr_size;
	const size_t prg_ram_size;
	const size_t prg_rom_size;

	const size_t num_chr_banks;
	const size_t num_prg_ram_banks;
	const size_t num_prg_rom_banks;

	bool chr_is_ram;
	bool has_prg_ram;
	bool mirroring;

	std::vector<u8> chr;
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;

	// Horizontal mirroring; addresses in $2400-$27FF and $2C00-$2FFF are transformed into $2000-$23FF and $2800-$2BFF, respectively.
	u16 NametableAddrHorizontal(u16 addr) const { return addr & ~0x400; }

	// Vertical mirroring; addresses in $2800-$2FFF are transformed into $2000-$27FF.
	u16 NametableAddrVertical(u16 addr) const { return addr & ~0x800; }

	// Single screen, lower; addresses in $2000-$2FFF are transformed into $2000-$23FF
	u16 NametableAddrSingleLower(u16 addr) const { return addr & ~0xC00; }

	// Single screen, upper; addresses in $2000-$2FFF are transformed into $2400-$27FF
	u16 NametableAddrSingleUpper(u16 addr) const { return addr & ~0x800 | 0x400; }

	// 4-Screen: address are not transformed
	u16 NametableAddrFourScreen(u16 addr) const { return addr; }
};

