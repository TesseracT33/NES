#include "Cartridge.h"


std::optional<std::shared_ptr<BaseMapper>> Cartridge::ConstructMapperFromRom(const std::string& rom_path)
{
	/* Attempt to open the rom file */
	std::ifstream rom_ifs{rom_path, std::ifstream::in | std::ifstream::binary};
	if (!rom_ifs)
	{
		UserMessage::Show("Failed to open rom file.", UserMessage::Type::Error);
		return std::nullopt;
	}

	/* Compute the rom file size */
	rom_ifs.seekg(0, rom_ifs.end);
	const size_t rom_size = rom_ifs.tellg();
	rom_ifs.seekg(0, rom_ifs.beg);

	/* Read and parse the rom header (16 bytes), containing properties of the cartridge/mapper. */
	std::array<u8, header_size> header{};
	MapperProperties mapper_properties{rom_path};
	rom_ifs.read((char*)&header[0], header_size);
	bool success = ParseHeader(header, mapper_properties);
	if (!success)
		return std::nullopt;

	/* Setup the various vectors (e.g.prg_rom, chr_rom) inside of the mapper, by reading the full rom file.
	   The rom layout is the following:    header | trainer (optional) | PRG ROM | CHR ROM    */
	const size_t trainer_size = 0x200;
	const size_t prg_rom_start = header_size + (mapper_properties.has_trainer ? trainer_size : 0);
	const size_t chr_prg_rom_size = rom_size - prg_rom_start;
	const size_t specified_chr_prg_rom_size = mapper_properties.chr_size + mapper_properties.prg_rom_size;

	// TODO: compare specified_chr_prg_rom_size with chr_prg_rom_size

	std::vector<u8> rom_vec{};
	rom_vec.resize(chr_prg_rom_size);
	rom_ifs.seekg(prg_rom_start);
	rom_ifs.read((char*)&rom_vec[0], chr_prg_rom_size);

	/* Match, construct and return a mapper. If the construction fails (e.g. due to unsupported mapper detected), return. */
	std::optional<std::shared_ptr<BaseMapper>> mapper = ConstructMapperFromMapperNumber(rom_vec, mapper_properties);
	if (!mapper.has_value())
		return std::nullopt;
	return mapper;
}


std::optional<std::shared_ptr<BaseMapper>> Cartridge::ConstructMapperFromMapperNumber(const std::vector<u8> rom_vec, MapperProperties& mapper_properties)
{
	auto Instantiate = [&] <typename Mapper> () -> std::optional<std::shared_ptr<BaseMapper>>
	{
		std::shared_ptr<BaseMapper> mapper = std::make_shared<Mapper>(rom_vec, mapper_properties);
		return std::make_optional<std::shared_ptr<BaseMapper>>(mapper);
	};

	switch (mapper_properties.mapper_num)
	{ /* The syntax is a bit strange, but this is just calling the lambda with a particular type. */
	case   0: return Instantiate.template operator() < NROM      > ();
	case   1: return Instantiate.template operator() < MMC1      > ();
	case   2: return Instantiate.template operator() < UxROM     > ();
	case   3: return Instantiate.template operator() < CNROM     > ();
	case   4: return Instantiate.template operator() < MMC3      > ();
	case   7: return Instantiate.template operator() < AxROM     > ();
	case  94: return Instantiate.template operator() < Mapper094 > ();
	case 180: return Instantiate.template operator() < Mapper180 > ();
	default:
		UserMessage::Show(std::format("Unsupported mapper number {} detected.", mapper_properties.mapper_num), UserMessage::Type::Error);
		return std::nullopt;
	}
}


/* Returns true on success */
bool Cartridge::ParseHeader(const std::array<u8, header_size>& header, MapperProperties& mapper_properties)
{
	// https://wiki.nesdev.org/w/index.php/NES_2.0#Identification
	/* Check if the header is a valid iNES header */
	if (!(header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A))
	{
		UserMessage::Show("Could not parse rom file header; rom is not a valid iNES or NES 2.0 image file.", UserMessage::Type::Error);
		return false;
	}

	/* The first eight bytes of both the iNES and NES 2.0 headers have the same meaning. */
	ParseFirstEightBytesOfHeader(header, mapper_properties);

	/* Check if the header is a valid NES 2.0 header. Then, parse bytes 8-15. */
	if ((header[7] & 0x0C) == 0x08)
		ParseNES20Header(header, mapper_properties);
	else
		ParseiNESHeader(header, mapper_properties);

	return true;
}


void Cartridge::ParseFirstEightBytesOfHeader(const std::array<u8, header_size>& header, MapperProperties& mapper_properties)
{
	/* Parse bytes 0-7 of the header. These have the same meaning on both iNES and NES 2.0 headers. */
	mapper_properties.prg_rom_size = header[4] * prg_rom_bank_size;

	/* Check CHR ROM size. A value of 0 means that the board uses CHR RAM. How much depends on the cart/mapper. */
	if (header[5] == 0)
	{
		mapper_properties.has_chr_ram = true;
		mapper_properties.chr_size = 0; /* To be found out from a potential NES 2.0 header, or set by the mapper. */
	}
	else
	{
		mapper_properties.has_chr_ram = false;
		mapper_properties.chr_size = header[5] * chr_bank_size;
	}

	mapper_properties.mirroring              = header[6] & 0x01;
	mapper_properties.has_persistent_prg_ram = header[6] & 0x02;
	mapper_properties.has_trainer            = header[6] & 0x04;
	mapper_properties.hard_wired_four_screen = header[6] & 0x08;

	mapper_properties.mapper_num = header[7] & 0xF0 | header[6] >> 4;
}


void Cartridge::ParseiNESHeader(const std::array<u8, header_size>& header, MapperProperties& mapper_properties)
{
	/* Parse bytes 8-15 of an iNES header. */
	mapper_properties.prg_ram_size = header[8];

	// Note: Dendy is not supported from this
	switch (header[9] & 1)
	{
	case 0: mapper_properties.video_standard = System::VideoStandard::NTSC; break;
	case 1: mapper_properties.video_standard = System::VideoStandard::PAL; break;
	}
}


void Cartridge::ParseNES20Header(const std::array<u8, header_size>& header, MapperProperties& mapper_properties)
{
	/* Parse bytes 8-15 of a NES 2.0 header. */
	mapper_properties.mapper_num |= (header[8] & 0xF) << 8;
	mapper_properties.submapper_num = header[8] >> 4;
	if (!mapper_properties.has_chr_ram)
		mapper_properties.chr_size += (size_t(header[9] & 0x0F) << 8) * chr_bank_size;
	mapper_properties.prg_rom_size += (size_t(header[9] & 0xF0) << 4) * prg_rom_bank_size;

	// Check for PRG-RAM
	if (header[10] & 0x0F)
		mapper_properties.prg_ram_size = size_t(64) << (header[10] & 0xF);
	else
		mapper_properties.has_prg_ram = false;

	// Check for PRG-NVRAM
	if (header[10] & 0xF0)
		mapper_properties.prg_nvram_size = size_t(64) << (header[10] >> 4);
	else
		mapper_properties.has_prg_nvram = false;

	// Check for CHR-RAM
	if (header[11] & 0x0F)
		mapper_properties.chr_size = size_t(64) << (header[11] & 0xF);
	else
		mapper_properties.has_chr_ram = false;

	// Check for CHR-NVRAM
	if (header[11] & 0xF0)
		mapper_properties.chr_nvram_size = size_t(64) << (header[11] >> 4);
	else
		mapper_properties.has_chr_nvram = false;

	switch (header[12] & 3)
	{
	case 0: mapper_properties.video_standard = System::VideoStandard::NTSC ; break;
	case 1: mapper_properties.video_standard = System::VideoStandard::PAL  ; break;
	case 2: mapper_properties.video_standard = System::VideoStandard::NTSC ; break; // TODO: this is actually "Multi-region"
	case 3: mapper_properties.video_standard = System::VideoStandard::Dendy; break;
	}
}