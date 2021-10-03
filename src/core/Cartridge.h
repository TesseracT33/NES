#pragma once

#include <optional>

#include "mappers/BaseMapper.h"
#include "mappers/MapperIncludes.h"

class Cartridge final
{
public:
	static std::optional<std::shared_ptr<BaseMapper>> ConstructMapperFromRom(const std::string& rom_path);

private:
	struct MapperInfo
	{
		bool has_chr_ram;
		bool hard_wired_four_screen;
		bool has_chr_nvram;
		bool has_prg_ram;
		bool has_prg_nvram;
		bool has_trainer;
		bool mirroring;
		u8 tv_system : 3;
		u8 submapper_num;
		u16 mapper_num;
		size_t chr_nvram_size;
		size_t chr_size; // refers to either rom or ram; a mapper can't use both (?)
		size_t prg_nvram_size;
		size_t prg_ram_size;
		size_t prg_rom_size;
	} static mapper_info;

	static std::shared_ptr<BaseMapper> mapper;

	static std::optional<std::shared_ptr<BaseMapper>> ConstructMapper();

	template<typename Mapper> static void MapperFactory() { 
		mapper = std::make_shared<Mapper>(
			mapper_info.chr_size,
			mapper_info.prg_rom_size, 
			mapper_info.has_prg_ram ? mapper_info.prg_ram_size : 0); }

	static bool ParseHeader(u8 header[]);
	static void ParseFirstEightBytesOfHeader(u8 header[]);
	static void ParseiNESHeader(u8 header[]);
	static void ParseNES20Header(u8 header[]);
	static void SetupMapperProperties();
};

