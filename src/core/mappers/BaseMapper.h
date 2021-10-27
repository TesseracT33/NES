#pragma once

#include <format>
#include <vector>

#include "MapperProperties.h"

#include "../CPU.h"
#include "../System.h"

#include "../../Types.h"

class BaseMapper
{
public:
	CPU* cpu;

	BaseMapper(MapperProperties _mapper_properties) : properties(_mapper_properties)
	{
		/* If CHR is RAM, then its size won't be given by the rom header. Instead, the mapper construct should take care of this. */
		if (properties.has_chr_ram)
			chr.resize(0x2000); /* Default behaviour for now: 8 KiB CHR RAM. */
		else
			chr.resize(properties.chr_size);

		/* If the cart has PRG RAM, then its size may not be given by the rom header. Instead, the mapper construct should take care of this. */
		if (properties.has_prg_ram)
			prg_ram.resize(0x2000); /* Default behaviour for now: 8 KiB PRG RAM. */
		else
			prg_ram.resize(properties.prg_ram_size);

		prg_rom.resize(properties.prg_rom_size);
	}

	void LayoutMemory(u8* rom_arr)
	{
		/* Copy ROM data into PRG and CHR ROM. */
		memcpy(&prg_rom[0], rom_arr, properties.prg_rom_size);
		memcpy(&chr[0], rom_arr + properties.prg_rom_size, properties.chr_size);
	}

	const System::VideoStandard GetVideoStandard() const { return properties.video_standard; };

	virtual u8 ReadPRG(u16 addr) = 0;
	virtual u8 ReadCHR(u16 addr) = 0;

	virtual void WritePRG(u16 addr, u8 data) {};
	virtual void WriteCHR(u16 addr, u8 data) {};

	virtual u16 GetNametableAddr(u16 addr) = 0;

	virtual void ClockIRQ() {};

protected:
	static constexpr size_t chr_bank_size     = 0x2000; /* These are the defaults; most supported mappers use them. */
	static constexpr size_t prg_ram_bank_size = 0x2000;
	static constexpr size_t prg_rom_bank_size = 0x4000;

	const MapperProperties properties;

	std::vector<u8> chr; /* Either RAM or ROM (a cart cannot have both). */
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;

	virtual constexpr size_t GetNumCHRBanks   () const { return properties.chr_size     / chr_bank_size    ; };
	virtual constexpr size_t GetNumPRGRAMBanks() const { return properties.prg_ram_size / prg_ram_bank_size; };
	virtual constexpr size_t GetNumPRGROMBanks() const { return properties.prg_rom_size / prg_rom_bank_size; };

	// Horizontal mirroring; addresses in $2400-$27FF and $2C00-$2FFF are transformed into $2000-$23FF and $2800-$2BFF, respectively.
	u16 NametableAddrHorizontal(u16 addr) const { return addr & ~0x400; }

	// Vertical mirroring; addresses in $2800-$2FFF are transformed into $2000-$27FF.
	u16 NametableAddrVertical(u16 addr) const { return addr & ~0x800; }

	// Single screen, lower; addresses in $2000-$2FFF are transformed into $2000-$23FF
	u16 NametableAddrSingleLower(u16 addr) const { return addr & ~0xC00; }

	// Single screen, upper; addresses in $2000-$2FFF are transformed into $2400-$27FF
	u16 NametableAddrSingleUpper(u16 addr) const { return addr & ~0x800 | 0x400; }

	// 4-Screen: address are not transformed
	u16 NametableAddrFourScreen(u16 addr) const { return addr; }
};

