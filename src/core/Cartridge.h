#pragma once

#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <vector>

#include "../gui/UserMessage.h"

#include "mappers/BaseMapper.h"
#include "mappers/MapperIncludes.h"
#include "mappers/MapperProperties.h"

/* This class is used to construct a mapper object given a rom file. */
class Cartridge final
{
public:
	static std::optional<std::unique_ptr<BaseMapper>> ConstructMapperFromRom(const std::string& rom_path);

private:
	/* The header will specify the rom size in units of the below. */
	static constexpr size_t chr_bank_size     = 0x2000;
	static constexpr size_t prg_ram_bank_size = 0x2000;
	static constexpr size_t prg_rom_bank_size = 0x4000;

	static constexpr size_t header_size = 0x10;

	static std::optional<std::unique_ptr<BaseMapper>> ConstructMapperFromMapperNumber(std::vector<u8> rom_vec, MapperProperties& mapper_properties);

	static bool ParseHeader(const std::array<u8, header_size>& header, MapperProperties& properties);
	static void ParseFirstEightBytesOfHeader(const std::array<u8, header_size>& header, MapperProperties& properties);
	static void ParseiNESHeader(const std::array<u8, header_size>& header, MapperProperties& properties);
	static void ParseNES20Header(const std::array<u8, header_size>& header, MapperProperties& properties);
};

