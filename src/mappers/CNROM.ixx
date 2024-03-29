export module CNROM;

import BaseMapper;
import MapperProperties;

import NumericalTypes;
import SerializationStream;

import <vector>;

export class CNROM : public BaseMapper
{
public:
	CNROM(std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, properties) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF) {
			return 0xFF;
		}
		// CPU $8000-$BFFF: First 16 KiB of ROM.
		else if (addr <= 0xBFFF) {
			return prg_rom[addr - 0x8000];
		}
		// CPU $C000-$FFFF: Last 16 KiB of ROM (CNROM-256) or mirror of $8000-$BFFF (CNROM-128).
		else {
			return prg_rom[addr - (properties.prg_rom_size == 0x4000 ? 0xC000 : 0x8000)];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000) {
			chr_bank = data % properties.num_chr_banks; // The CHR capacity is at most 32 KiB (four 8 KiB banks). chr_bank is 2 bits.
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB switchable CHR ROM bank.
		return chr[addr + 0x2000 * chr_bank];
	};

	virtual void StreamState(SerializationStream& stream) override
	{
		BaseMapper::StreamState(stream);
		chr_bank = stream.StreamBitfield(chr_bank);
	};

protected:
	uint chr_bank : 2 = 0;
};