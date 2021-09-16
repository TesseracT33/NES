#pragma once

#include <vector>

#include "../Header.h"

#include "../../Types.h"

class BaseMapper
{
public:
	virtual void Initialize() {}

	virtual u8   ReadPRG(u16 addr) const = 0;
	virtual void WritePRG(u16 addr, u8 data) = 0;

	virtual u8   ReadCHR (u16 addr) const = 0;
	virtual void WriteCHR(u16 addr, u8 data) = 0;

	virtual u16 GetNametableAddr(u16 addr) { return addr; }

protected:
	friend class Cartridge;

	const size_t chr_piece_size = 0x4000;
	const size_t prg_piece_size = 0x4000;

	bool chr_is_ram;
	bool mirroring;

	std::vector<u8> chr_rom;
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;

	u8 GetNumPRGRomBanks() const { return prg_rom.size() / prg_piece_size; }
	u8 GetNumCHRRomBanks() const { return chr_rom.size() / chr_piece_size; }
};

