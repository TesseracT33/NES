module Cartridge;

import AxROM;
import CNROM;
import Mapper094;
import Mapper180;
import MapperProperties;
import MMC1;
import MMC3;
import NROM;
import System;
import UxROM;

import Util.Files;

import UserMessage;

namespace Cartridge
{
	void Eject()
	{
		mapper.release();
	}


	bool LoadRom(const std::string& path)
	{
		std::optional<std::vector<u8>> opt_rom = Util::Files::LoadBinaryFileVec(path);
		if (!opt_rom.has_value()) {
			UserMessage::Show(std::format("Could not open file at {}", path), UserMessage::Type::Error);
			return false;
		}
		std::vector<u8> rom = opt_rom.value();
		/* Read and parse the rom header (16 bytes), containing properties of the cartridge/mapper. */
		Header header{};
		MapperProperties mapper_properties{ path };
		std::copy(rom.begin(), rom.begin() + header_size, header.begin());
		bool success = ParseHeader(header, mapper_properties);
		if (!success) {
			return false;
		}
		/* Setup the various vectors (e.g.prg_rom, chr_rom) inside of the mapper, by reading the full rom file.
		   The rom layout is the following:    header | trainer (optional) | PRG ROM | CHR ROM    */
		constexpr size_t trainer_size = 0x200;
		size_t prg_rom_start = header_size + (mapper_properties.has_trainer ? trainer_size : 0);
		size_t chr_prg_rom_size = rom.size() - prg_rom_start;
		size_t header_specified_chr_prg_rom_size = mapper_properties.chr_size + mapper_properties.prg_rom_size;
		/* Compare the rom size specified by the header and the one that we found from reading the actual rom file. */
		if (header_specified_chr_prg_rom_size != chr_prg_rom_size) {
			UserMessage::Show(std::format(
				"Rom size mismatch; rom file (PRG+CHR) was {} bytes large, but the header specified it as {} bytes large",
				chr_prg_rom_size, header_specified_chr_prg_rom_size), UserMessage::Type::Error);
			return false;
		}
		/* Remove the header and the potential trainer from the rom vector */
		size_t index = 0;
		while (index++ < prg_rom_start) {
			rom.erase(rom.begin());
		}
		/* Construct a mapper. */
#define MAKE_MAPPER(MAPPER) std::make_unique<MAPPER>(rom, mapper_properties)
		switch (mapper_properties.mapper_num) {
		case   0: mapper = MAKE_MAPPER(NROM); break;
		case   1: mapper = MAKE_MAPPER(MMC1); break;
		case   2: mapper = MAKE_MAPPER(UxROM); break;
		case   3: mapper = MAKE_MAPPER(CNROM); break;
		case   4: mapper = MAKE_MAPPER(MMC3); break;
		case   7: mapper = MAKE_MAPPER(AxROM); break;
		case  94: mapper = MAKE_MAPPER(Mapper094); break;
		case 180: mapper = MAKE_MAPPER(Mapper180); break;
		default:
			UserMessage::Show(std::format("Unsupported mapper number {} detected.", mapper_properties.mapper_num), UserMessage::Type::Error);
		}
#undef MAKE_MAPPER

		return mapper != nullptr;
	}


	bool Cartridge::ParseHeader(const Header& header, MapperProperties& mapper_properties)
	{
		// https://wiki.nesdev.org/w/index.php/NES_2.0#Identification
		/* Check if the header is a valid iNES header */
		if (!(header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A)) {
			UserMessage::Show("Could not parse rom file header; rom is not a valid iNES or NES 2.0 image file.", UserMessage::Type::Error);
			return false;
		}
		/* The first eight bytes of both the iNES and NES 2.0 headers have the same meaning. */
		ParseFirstEightBytesOfHeader(header, mapper_properties);
		/* Check if the header is a valid NES 2.0 header. Then, parse bytes 8-15. */
		if ((header[7] & 0x0C) == 0x08) {
			ParseNES20Header(header, mapper_properties);
		}
		else {
			ParseiNESHeader(header, mapper_properties);
		}
		return true;
	}


	void Cartridge::ParseFirstEightBytesOfHeader(const Header& header, MapperProperties& mapper_properties)
	{
		/* Parse bytes 0-7 of the header. These have the same meaning on both iNES and NES 2.0 headers. */
		mapper_properties.prg_rom_size = header[4] * prg_rom_bank_size;
		/* Check CHR ROM size. A value of 0 means that the board uses CHR RAM. How much depends on the cart/mapper. */
		if (header[5] == 0) {
			mapper_properties.has_chr_ram = true;
			mapper_properties.chr_size = 0; /* To be found out from a potential NES 2.0 header, or set by the mapper. */
		}
		else {
			mapper_properties.has_chr_ram = false;
			mapper_properties.chr_size = header[5] * chr_bank_size;
		}
		mapper_properties.mirroring = header[6] & 0x01;
		mapper_properties.has_persistent_prg_ram = header[6] & 0x02;
		mapper_properties.has_trainer = header[6] & 0x04;
		mapper_properties.hard_wired_four_screen = header[6] & 0x08;
		mapper_properties.mapper_num = header[7] & 0xF0 | header[6] >> 4;
	}


	void Cartridge::ParseiNESHeader(const Header& header, MapperProperties& mapper_properties)
	{
		/* Parse bytes 8-15 of an iNES header. */
		mapper_properties.prg_ram_size = header[8];
		// Note: Dendy is not supported from this
		mapper_properties.standard = [&] {
			if (header[9] & 1) {
				return System::standard_pal;
			}
			else {
				return System::standard_ntsc;
			}
		}();
		System::standard = mapper_properties.standard;
	}


	void Cartridge::ParseNES20Header(const Header& header, MapperProperties& mapper_properties)
	{
		/* Parse bytes 8-15 of a NES 2.0 header. */
		mapper_properties.mapper_num |= (header[8] & 0xF) << 8;
		mapper_properties.submapper_num = header[8] >> 4;
		if (!mapper_properties.has_chr_ram) { /* If the cart has CHR RAM, its size if not provided by the header. */
			mapper_properties.chr_size += (size_t(header[9] & 0x0F) << 8) * chr_bank_size;
		}
		mapper_properties.prg_rom_size += (size_t(header[9] & 0xF0) << 4) * prg_rom_bank_size;

		// Check for PRG-RAM
		if (header[10] & 0x0F) {
			mapper_properties.prg_ram_size = size_t(64) << (header[10] & 0xF);
		}
		else {
			mapper_properties.has_prg_ram = false;
		}
		// Check for PRG-NVRAM
		if (header[10] & 0xF0) {
			mapper_properties.prg_nvram_size = size_t(64) << (header[10] >> 4);
		}
		else {
			mapper_properties.has_prg_nvram = false;
		}
		// Check for CHR-RAM
		if (header[11] & 0x0F) {
			mapper_properties.chr_size = size_t(64) << (header[11] & 0xF);
		}
		else {
			mapper_properties.has_chr_ram = false;
		}
		// Check for CHR-NVRAM
		if (header[11] & 0xF0) {
			mapper_properties.chr_nvram_size = size_t(64) << (header[11] >> 4);
		}
		else {
			mapper_properties.has_chr_nvram = false;
		}

		mapper_properties.standard = [&] {
			switch (header[12] & 3) {
			case 0: return System::standard_ntsc;
			case 1: return System::standard_pal;
			case 2: return System::standard_ntsc; // TODO: this is actually "Multi-region"
			case 3: return System::standard_dendy;
			default: std::unreachable();
			}
		}();
		System::standard = mapper_properties.standard;
	}


	void StreamState(SerializationStream& stream)
	{
		mapper->StreamState(stream);
	}


	void ClockIRQ()
	{
		mapper->ClockIRQ();
	}


	u8 ReadNametableRAM(u16 addr)
	{
		return mapper->ReadNametableRAM(addr);
	}


	u8 ReadCHR(u16 addr)
	{
		return mapper->ReadCHR(addr);
	}


	u8 ReadPRG(u16 addr)
	{
		return mapper->ReadPRG(addr);
	}


	void ReadPRGRAMFromDisk()
	{
		mapper->ReadPRGRAMFromDisk();
	}


	void WriteCHR(u16 addr, u8 data)
	{
		mapper->WriteCHR(addr, data);
	}


	void WriteNametableRAM(u16 addr, u8 data)
	{
		mapper->WriteNametableRAM(addr, data);
	}


	void WritePRG(u16 addr, u8 data)
	{
		mapper->WritePRG(addr, data);
	}


	void WritePRGRAMToDisk()
	{
		mapper->WritePRGRAMToDisk();
	}
}