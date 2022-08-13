export module Mapper180;

import MapperProperties;
import UxROM;

import NumericalTypes;
import SerializationStream;

import <vector>;

export class Mapper180 : public UxROM
{
public:
	Mapper180(std::vector<u8> chr_prg_rom, MapperProperties properties) :
		UxROM(chr_prg_rom, properties) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF) {
			return 0xFF;
		}
		// CPU $8000-$BFFF: 16 KiB switchable PRG ROM bank
		else if (addr <= 0xBFFF) {
			return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
		}
		// CPU $C000-$FFFF: 16 KiB PRG ROM bank, fixed to the first bank
		else {
			return prg_rom[addr - 0xC000];
		}
	};
};