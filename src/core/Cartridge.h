#pragma once

#include <wx/msgdlg.h>

#include <array>

#include "Bus.h"

class Cartridge : public Component
{
public:
	enum class Mapper : u8
	{
		NROM,
		MMC1,
		UxROM,
		iNES_Mapper_003,
		MMC3,
		MMC5,
		iNES_Mapper_006,
		AxROM,
		iNES_Mapper_008,
		MMC2,
		MMC4,
		INVALID = 0xFF
	};

	static Mapper GetMapperFromCartridge(const char* rom_path);
	bool LoadCartridge(const char* rom_path);

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

	u8 Read(u16 addr, bool ppu = false);
	void Write(u16 addr, u8 data, bool ppu = false);

protected:
	enum Addr
	{
		TITLE_START = 0,
		PRG_ROM_SIZE = 4,
		CHR_ROM_SIZE = 5,
		FLAGS_6 = 6,
		FLAGS_7 = 7,
		FLAGS_8 = 8,
		FLAGS_9 = 9,
		FLAGS_10 = 10
	};

	static const size_t header_size = 0x10;
	static const size_t trainer_size = 0x200;
	static const size_t prg_piece_size = 0x4000;
	static const size_t chr_piece_size = 0x4000;

	u8 header[header_size];
	u8 trainer[trainer_size];

	u8 prg_size, chr_size;
};

