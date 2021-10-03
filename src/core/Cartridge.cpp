#include <wx/msgdlg.h>
#include "Cartridge.h"


Cartridge::MapperInfo Cartridge::mapper_info{};
std::shared_ptr<BaseMapper> Cartridge::mapper{};


std::optional<std::shared_ptr<BaseMapper>> Cartridge::ConstructMapperFromRom(const std::string& rom_path)
{
	FILE* rom_file = fopen(rom_path.c_str(), "rb");
	if (rom_file == NULL)
	{
		wxMessageBox("Failed to open rom file.");
		return std::nullopt;
	}

	fseek(rom_file, 0, SEEK_END);
	const size_t rom_size = ftell(rom_file);
	rewind(rom_file);

	// Read and parse the rom header
	const size_t header_size = 0x10;
	u8 header[header_size];
	fread(header, 1, header_size, rom_file);
	bool success = ParseHeader(header);
	if (!success)
		return std::nullopt;

	// Match and construct a mapper. If it fails (e.g. due to unsupported mapper detected), return.
	std::optional<std::shared_ptr<BaseMapper>> mapper = ConstructMapper();
	if (!mapper.has_value())
		return std::nullopt;

	// Setup the various vectors (e.g. prg_rom, chr_rom) inside of the mapper, by reading the full rom file
	const size_t trainer_size = 0x200;
	const size_t read_start = header_size + (mapper_info.has_trainer ? trainer_size : 0);
	const size_t chr_prg_rom_size = rom_size - read_start;
	u8* rom_arr = new u8[chr_prg_rom_size];
	fseek(rom_file, read_start, SEEK_SET);
	fread(rom_arr, 1, chr_prg_rom_size, rom_file);
	mapper->get()->LayoutMemory(rom_arr);
	delete[] rom_arr;
	fclose(rom_file);

	// Setup various properties of the mapper
	SetupMapperProperties();

	return mapper;
}


std::optional<std::shared_ptr<BaseMapper>> Cartridge::ConstructMapper()
{
	switch (mapper_info.mapper_num)
	{
	case 0x00: MapperFactory<NROM>(); break;
	case 0x01: MapperFactory<MMC1>(); break;
	case 0x02: MapperFactory<UxROM>(); break;
	case 0x03: MapperFactory<CNROM>(); break;
	case 0x04: MapperFactory<MMC3>(); break;
	case 0x07: MapperFactory<AxROM>(); break;
	default:
		wxMessageBox(wxString::Format("Unsupported mapper no. %u detected.", mapper_info.mapper_num));
		return std::nullopt;
	}
	return std::make_optional<std::shared_ptr<BaseMapper>>(mapper);
}


bool Cartridge::ParseHeader(u8 header[])
{
	// https://wiki.nesdev.com/w/index.php/NES_2.0#Identification
	/* Check if the header is a valid iNES header */
	if (!(header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A))
	{
		wxMessageBox("Error: Could not parse rom file header; rom is not a valid iNES or NES 2.0 image file.");
		return false;
	}

	/* The first eight bytes of both the iNES and NES 2.0 headers have the same meaning. */
	ParseFirstEightBytesOfHeader(header);

	/* Check if the header is a valid NES 2.0 header. Then, parse bytes 8-15. */
	if ((header[7] & 0x0C) == 0x08)
		ParseNES20Header(header);
	else
		ParseiNESHeader(header);

	return true;
}


void Cartridge::ParseFirstEightBytesOfHeader(u8 header[])
{
	/* Parse bytes 0-7 of the header. These have the same meaning on both iNES and NES 2.0 headers. */
	mapper_info.prg_rom_size = header[4] * BaseMapper::prg_rom_bank_size;
	if (header[5] == 0)
	{
		mapper_info.chr_size = BaseMapper::chr_bank_size; // TODO: not correct
		mapper_info.has_chr_ram = true;
	}
	else
	{
		mapper_info.chr_size = header[5] * BaseMapper::chr_bank_size;
		mapper_info.has_chr_ram = false;
	}

	mapper_info.mirroring = header[6] & 0x01;
	mapper_info.has_prg_ram = header[6] & 0x02;
	mapper_info.has_trainer = header[6] & 0x04;
	mapper_info.hard_wired_four_screen = header[6] & 0x08;

	mapper_info.mapper_num = header[7] & 0xF0 | header[6] >> 4;
}


void Cartridge::ParseiNESHeader(u8 header[])
{
	/* Parse bytes 8-15 of an iNES header. */
	mapper_info.prg_ram_size = header[8];
	mapper_info.tv_system = header[9] & 1;
}


void Cartridge::ParseNES20Header(u8 header[])
{
	/* Parse bytes 8-15 of a NES 2.0 header. */
	mapper_info.mapper_num |= (header[8] & 0xF) << 8;
	mapper_info.submapper_num = header[8] >> 4;
	if (!mapper_info.has_chr_ram)
		mapper_info.chr_size += ((header[9] & 0x0F) << 8) * BaseMapper::chr_bank_size;
	mapper_info.prg_rom_size += ((header[9] & 0xF0) << 4) * BaseMapper::prg_rom_bank_size;

	// Check for PRG-RAM
	if (header[10] & 0x0F)
		mapper_info.prg_ram_size = 64 << (header[10] & 0xF);
	else
		mapper_info.has_prg_ram = false;

	// Check for PRG-NVRAM
	if (header[10] & 0xF0)
		mapper_info.prg_nvram_size = 64 << (header[10] >> 4);
	else
		mapper_info.has_prg_nvram = false;

	// Check for CHR-RAM
	if (header[11] & 0x0F)
		mapper_info.chr_size = 64 << (header[11] & 0xF);
	else
		mapper_info.has_chr_ram = false;

	// Check for CHR-NVRAM
	if (header[11] & 0xF0)
		mapper_info.chr_nvram_size = 64 << (header[11] >> 4);
	else
		mapper_info.has_chr_nvram = false;

	mapper_info.tv_system = header[12] & 3;
}


void Cartridge::SetupMapperProperties()
{
	mapper->has_chr_ram = mapper_info.has_chr_ram;
	mapper->has_prg_ram = mapper_info.has_prg_ram;
	mapper->mirroring = mapper_info.mirroring;
}