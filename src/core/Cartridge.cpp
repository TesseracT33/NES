#include "Cartridge.h"

using namespace Util;

Cartridge::Mapper Cartridge::GetMapperFromCartridge(const char* rom_path)
{
	FILE* rom_file = fopen(rom_path, "rb");
	if (rom_file == NULL)
	{
		wxMessageBox("Failed to open rom file.");
		return Mapper::INVALID;
	}

	fseek(rom_file, 0, SEEK_END);
	rewind(rom_file);

	u8 header[0x10];
	fread(header, 1, header_size, rom_file);
	fclose(rom_file);

	u8 prg_size = header[Addr::PRG_ROM_SIZE];
	u8 chr_size = header[Addr::CHR_ROM_SIZE];

	u8 flags_6 = header[Addr::FLAGS_6];
	u8 flags_7 = header[Addr::FLAGS_7];

	u8 mapper_no = flags_7 & 0xF0 | (flags_6 & 0xF0) >> 4;

	bool valid_no = true;
	if (!valid_no)
		return Mapper::INVALID;

	return static_cast<Mapper>(mapper_no);
}


bool Cartridge::LoadCartridge(const char* rom_path)
{
	FILE* rom_file = fopen(rom_path, "rb");
	if (rom_file == NULL)
	{
		wxMessageBox("Failed to open rom file.");
		return false;
	}

	fseek(rom_file, 0, SEEK_END);
	rewind(rom_file);

	fread(header, 1, header_size, rom_file);

	u8 prg_size = header[Addr::PRG_ROM_SIZE];
	u8 chr_size = header[Addr::CHR_ROM_SIZE];

	//rom = Cartridge::ROM<prg_size, chr_size>();

	u8 flags_6 = header[Addr::FLAGS_6];
	u8 flags_7 = header[Addr::FLAGS_7];

	if (CheckBit(flags_6, 2) == 1)
		fread(trainer, 1, trainer_size, rom_file);

	u8 mapper_no = flags_7 & 0xF0 | (flags_6 & 0xF0) >> 4;

	fclose(rom_file);

	return true;
}

u8 Cartridge::Read(u16 addr, bool ppu)
{
	if (addr <= 0x5FFF)
	{

	}

	else if (addr <= 0x7FFF)
	{

	}

	else if (addr <= 0xBFFF)
	{

	}

	else
	{

	}

	return 0xFF;
}

void Cartridge::Write(u16 addr, u8 data, bool ppu)
{

}
