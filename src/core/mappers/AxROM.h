#pragma once

#include "BaseMapper.h"

class AxROM : public BaseMapper
{
public:
	AxROM(const std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, MutateProperties(properties)) {}

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x7FFF)
		{
			return 0xFF;
		}
		// CPU $8000-$BFFF: 32 KiB switchable PRG ROM bank
		return prg_rom[addr - 0x8000 + prg_bank * 0x8000];
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr >= 0x8000)
		{
			prg_bank = (data & 0x07) % properties.num_prg_rom_banks; /* Select 32 KiB PRG ROM bank */
			vram_page = data & 0x10;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// PPU $0000-$1FFF: 8 KiB RAM (not bank switched)
		return chr[addr];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		chr[addr] = data;
	};

	const std::array<int, 4>& GetNametableMap() const override
	{
		if (vram_page == 0)
			return nametable_map_singlescreen_bottom;
		return nametable_map_singlescreen_top;
	};

	void StreamState(SerializationStream& stream) override
	{
		BaseMapper::StreamState(stream);
		stream.StreamPrimitive(vram_page);
		prg_bank = stream.StreamBitfield(prg_bank);
	};

protected:
	bool vram_page = 0;
	unsigned prg_bank : 3 = 0;

private:
	static MapperProperties MutateProperties(MapperProperties properties)
	{
		SetCHRRAMSize(properties, 0x2000);
		SetPRGROMBankSize(properties, 0x8000);
		return properties;
	};
};