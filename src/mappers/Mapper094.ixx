export module Mapper094;

import MapperProperties;
import UxROM;

import NumericalTypes;
import SerializationStream;

import <vector>;

export class Mapper094 : public UxROM
{
public:
	Mapper094(std::vector<u8> chr_prg_rom, MapperProperties properties) :
		UxROM(chr_prg_rom, properties) {}

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000) {
			prg_bank = (data >> 2) % properties.num_prg_rom_banks;
		}
	};
};