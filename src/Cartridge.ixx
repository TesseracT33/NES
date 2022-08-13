export module Cartridge;

import BaseMapper;
import MapperProperties;

import NumericalTypes;
import SerializationStream;

import <algorithm>;
import <array>;
import <format>;
import <memory>;
import <optional>;
import <string>;
import <vector>;

class BaseMapper;

namespace Cartridge
{
	export
	{
		void ClockIRQ();
		void Eject();
		bool LoadRom(const std::string& path);
		u8 ReadNametableRAM(u16 addr);
		u8 ReadCHR(u16 addr);
		u8 ReadPRG(u16 addr);
		void ReadPRGRAMFromDisk();
		void StreamState(SerializationStream& stream);
		void WriteCHR(u16 addr, u8 data);
		void WriteNametableRAM(u16 addr, u8 data);
		void WritePRG(u16 addr, u8 data);
		void WritePRGRAMToDisk();
	}

	/* The header will specify the rom size in units of the below. */
	constexpr size_t chr_bank_size = 0x2000;
	constexpr size_t prg_ram_bank_size = 0x2000;
	constexpr size_t prg_rom_bank_size = 0x4000;
	constexpr size_t header_size = 0x10;

	using Header = std::array<u8, header_size>;

	bool ParseHeader(const Header& header, MapperProperties& properties);
	void ParseFirstEightBytesOfHeader(const Header& header, MapperProperties& properties);
	void ParseiNESHeader(const Header& header, MapperProperties& properties);
	void ParseNES20Header(const Header& header, MapperProperties& properties);

	std::unique_ptr<BaseMapper> mapper;
}