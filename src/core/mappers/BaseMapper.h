#pragma once

#include <vector>

#include "../../Types.h"

class BaseMapper
{
public:
	virtual u8 Read(u16 addr) const = 0;
	virtual void Write(u16 addr, u8 data) = 0;

protected:
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;
	std::vector<u8> chr_rom;

	friend class Cartridge;
};

