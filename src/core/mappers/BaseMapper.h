#pragma once

#include <algorithm>
#include <format>
#include <vector>

#include "MapperProperties.h"

#include "../Component.h"
#include "../System.h"

#include "../../Types.h"

#include "../../gui/AppUtils.h"

class BaseMapper : public Component
{
public:
	using Component::Component;

	BaseMapper(const std::vector<u8> chr_prg_rom, MapperProperties properties) : properties(properties)
	{
		/* These must be calculated here, and cannot be part of the properties passed to the submapper constructor,
		   as bank sizes are not known before the submapper constructors have been called. */
		this->properties.num_chr_banks = properties.chr_size / properties.chr_bank_size;
		this->properties.num_prg_ram_banks = properties.prg_ram_size / properties.prg_ram_bank_size;
		this->properties.num_prg_rom_banks = properties.prg_rom_size / properties.prg_rom_bank_size;

		/* Resize all vectors */
		chr.resize(properties.chr_size);
		prg_ram.resize(properties.prg_ram_size);
		prg_rom.resize(properties.prg_rom_size);

		/* Fill vectors with either rom data or $00 */
		std::copy(chr_prg_rom.begin(), chr_prg_rom.begin() + properties.prg_rom_size, prg_rom.begin());

		if (!properties.has_chr_ram)
			std::copy(chr_prg_rom.begin() + properties.prg_rom_size, chr_prg_rom.end(), chr.begin());
		else
			std::fill(chr.begin(), chr.end(), 0x00);

		if (!prg_ram.empty())
			std::fill(prg_ram.begin(), prg_ram.end(), 0x00);

		for (auto& nametable_arr : nametable_ram)
			nametable_arr.fill(0x00);
	}

	const System::VideoStandard GetVideoStandard() const { return properties.video_standard; };

	void ReadPRGRAMFromDisk()
	{
		if (properties.has_persistent_prg_ram)
		{
			const std::string save_data_path = properties.rom_path + save_file_postfix;
			if (AppUtils::FileExists(save_data_path))
			{
				std::ifstream ifs{ save_data_path, std::ifstream::in | std::ofstream::binary };
				if (!ifs)
				{
					UserMessage::Show("Save file loading failed!");
					return;
				}
				ifs.read((char*)prg_ram.data(), prg_ram.size());
			}
		}
	}

	void WritePRGRAMToDisk() const
	{
		if (properties.has_persistent_prg_ram)
		{
			static bool save_data_creation_has_failed = false;
			if (!save_data_creation_has_failed) /* Avoid the spamming of user messages, since this function is called regularly. */
			{
				const std::string save_data_path = properties.rom_path + save_file_postfix;
				std::ofstream ofs{ save_data_path, std::ofstream::out | std::ofstream::binary };
				if (!ofs)
				{
					save_data_creation_has_failed = true;
					UserMessage::Show("Save file creation failed!");
					return;
				}
				ofs.write((const char*)prg_ram.data(), prg_ram.size());
			}
		}
	}

	virtual u8 ReadPRG(u16 addr) = 0;
	virtual u8 ReadCHR(u16 addr) = 0;

	virtual void WritePRG(u16 addr, u8 data) {};
	virtual void WriteCHR(u16 addr, u8 data) {};

	u8 ReadNametableRAM(u16 addr)
	{
		const int page = GetNametablePage(addr);
		return nametable_ram[page][addr & 0x3FF];
	}

	void WriteNametableRAM(u16 addr, u8 data)
	{
		const int page = GetNametablePage(addr);
		nametable_ram[page][addr & 0x3FF] = data;
	}

	virtual void ClockIRQ() {};

	/* This function should always be called from the derived classes' 'StreamState' functions. */
	void StreamState(SerializationStream& stream) override
	{
		stream.StreamArray(nametable_ram);
		stream.StreamVector(prg_ram);
		if (properties.has_chr_ram)
			stream.StreamVector(chr);
	}

protected:
	/* https://wiki.nesdev.org/w/index.php/Mirroring */
	static constexpr std::array<int, 4> nametable_map_horizontal          = { 0, 0, 1, 1 };
	static constexpr std::array<int, 4> nametable_map_vertical            = { 0, 1, 0, 1 };
	static constexpr std::array<int, 4> nametable_map_singlescreen_bottom = { 0, 0, 0, 0 };
	static constexpr std::array<int, 4> nametable_map_singlescreen_top    = { 1, 1, 1, 1 };
	static constexpr std::array<int, 4> nametable_map_fourscreen          = { 1, 2, 3, 4 };
	static constexpr std::array<int, 4> nametable_map_diagonal            = { 1, 2, 2, 1 };

	const std::string save_file_postfix = "_SAVE_DATA.bin";

	MapperProperties properties;

	std::vector<u8> chr; /* Either RAM or ROM (a cart cannot have both). */
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;

	virtual const std::array<int, 4>& GetNametableMap() const
	{
		if (properties.mirroring == 0)
			return nametable_map_horizontal;
		return nametable_map_vertical;
	}

	/* The following static functions may be called from submapper constructors.
	   The submapper classes must apply these properties themselves; they cannot be deduced from the rom header. */
	static void SetCHRBankSize(MapperProperties& properties, size_t size)
	{
		properties.chr_bank_size = size;
	}

	static void SetPRGRAMBankSize(MapperProperties& properties, size_t size)
	{
		properties.prg_ram_bank_size = size;
	}

	static void SetPRGROMBankSize(MapperProperties& properties, size_t size)
	{
		properties.prg_rom_bank_size = size;
	}

	/* A submapper constructor must call this function if it has CHR RAM, because if it has RAM instead of ROM,
	   the CHR size specified in the rom header will always (?) be 0. */
	static void SetCHRRAMSize(MapperProperties& properties, size_t size)
	{
		if (properties.has_chr_ram && properties.chr_size == 0)
			properties.chr_size = size;
	}

	/* The PRG RAM size (or PRG RAM presence) may or may not be specified in the rom header,
	   in particular if using iNES and not NES 2.0.
	   For now, let games with mappers that support PRG RAM, always have PRG RAM of some predefined size. */
	static void SetPRGRAMSize(MapperProperties& properties, size_t size)
	{
		if (properties.prg_ram_size == 0)
		{
			properties.has_prg_ram = true;
			properties.prg_ram_size = size;
		}
	}

private:
	std::array<std::array<u8, 0x400>, 4> nametable_ram{};

	int GetNametablePage(u16 addr) const
	{
		const std::array<int, 4>& map = GetNametableMap();
		const int quadrant = (addr & 0xF00) >> 10; /* $2000-$23FF ==> 0; $2400-$27FF ==> 1; $2800-$2BFF ==> 2; $2C00-$2FFF ==> 3 */
		const int page = map[quadrant];
		return page;
	}
};

