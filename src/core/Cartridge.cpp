#include "Cartridge.h"


void Cartridge::Initialize()
{

}


void Cartridge::Reset()
{

}


void Cartridge::Eject()
{

}


u8 Cartridge::Read(u16 addr, bool ppu) const { return mapper->ReadPRG(addr); }


void Cartridge::Write(u16 addr, u8 data, bool ppu) { mapper->WritePRG(addr, data); }


bool Cartridge::ReadRomFile(std::string path)
{
	FILE* rom_file = fopen(path.c_str(), "rb");
	if (rom_file == NULL)
	{
		wxMessageBox("Failed to open rom file.");
		return false;
	}

	fseek(rom_file, 0, SEEK_END);
	size_t rom_size = ftell(rom_file);
	rewind(rom_file);

	// parse the rom header and put details about the rom into var 'header'
	u8 header_arr[header_size];
	fread(header_arr, 1, header_size, rom_file);
	ParseRomHeader(header_arr);

	ConstructMapper();

	// setup the various vectors (e.g. prg_rom, chr_rom) inside of the mapper, by reading the full rom file
	rewind(rom_file);
	u8* rom_arr = new u8[rom_size];
	fread(rom_arr, 1, rom_size, rom_file);
	LayoutMapperMemory(rom_arr);
	delete[] rom_arr;

	fclose(rom_file);

	return true;
}


void Cartridge::ParseRomHeader(u8* header_arr)
{
	enum HeaderAddr
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

	this->header.prg_size = header_arr[PRG_ROM_SIZE] * prg_piece_size;
	this->header.chr_size = header_arr[CHR_ROM_SIZE] * chr_piece_size;
	this->header.has_trainer = header_arr[FLAGS_6] & 4;
	this->header.mapper_num = header_arr[FLAGS_7] & 0xF0 | header_arr[FLAGS_6] >> 4;
}


void Cartridge::ConstructMapper()
{
	switch (this->header.mapper_num)
	{
	case 0x00: MapperFactory<NROM>() ; break;
	case 0x01: MapperFactory<MMC1>() ; break;
	case 0x02: MapperFactory<UxROM>(); break;
	}

	ppu->mapper = this->mapper;
}


void Cartridge::LayoutMapperMemory(u8* rom_arr)
{
	mapper->prg_rom.resize(header.prg_size);
	mapper->chr_rom.resize(header.chr_size);

	u16 prg_rom_addr_start = header_size + (header.has_trainer ? trainer_size : 0);
	memcpy(&mapper->prg_rom[0], rom_arr + prg_rom_addr_start, header.prg_size);

	u16 chr_rom_addr_start = prg_rom_addr_start + header.prg_size;
	memcpy(&mapper->chr_rom[0], rom_arr + chr_rom_addr_start, header.chr_size);

	mapper->Initialize();
}


void Cartridge::State(Serialization::BaseFunctor& functor)
{

}