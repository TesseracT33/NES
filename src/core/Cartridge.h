#pragma once

#include <optional>

#include "mappers/BaseMapper.h"
#include "mappers/MapperIncludes.h"
#include "mappers/MapperProperties.h"

class Cartridge final
{
public:
	static std::optional<std::shared_ptr<BaseMapper>> ConstructMapperFromRom(const std::string& rom_path);

private:
	static const size_t chr_bank_size     = 0x2000;
	static const size_t prg_ram_bank_size = 0x2000;
	static const size_t prg_rom_bank_size = 0x4000;

	static MapperProperties mapper_properties;

	static std::shared_ptr<BaseMapper> mapper;

	static std::optional<std::shared_ptr<BaseMapper>> ConstructMapper();

	template<typename Mapper> static void MapperFactory()
	{ 
		mapper = std::make_shared<Mapper>(mapper_properties);
	}

	static bool ParseHeader(u8 header[]);
	static void ParseFirstEightBytesOfHeader(u8 header[]);
	static void ParseiNESHeader(u8 header[]);
	static void ParseNES20Header(u8 header[]);
};

