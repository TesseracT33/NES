#pragma once

#include "BaseMapper.h"

class MMC3 final : public BaseMapper
{
public:
	MMC3(MapperProperties mapper_properties) : BaseMapper(mapper_properties) {}

	u8 ReadPRG(u16 addr) override
	{
		switch (addr >> 12)
		{
		// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		case 0x6: case 0x7:
			// Disabling PRG RAM causes reads from the PRG RAM region to return open bus.
			if (properties.has_prg_ram && prg_ram_enabled)
				return value_last_read_from_prg_ram = prg_ram[addr - 0x6000];
			return value_last_read_from_prg_ram;

		// CPU $8000-$9FFF (or $C000-$DFFF): 8 KiB switchable PRG ROM bank
		case 0x8: case 0x9:
			if (prg_rom_bank_mode == 0)
				return prg_rom[addr + 0x2000 * rom_bank[6] - 0x8000];
			return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 2) - 0x8000];

		// CPU $A000-$BFFF: 8 KiB switchable PRG ROM bank
		case 0xA: case 0xB:
			return prg_rom[addr + 0x2000 * rom_bank[7] - 0xA000];

		// CPU $C000-$DFFF (or $8000-$9FFF): 8 KiB PRG ROM bank, fixed to the second-last bank
		case 0xC: case 0xD:
			if (prg_rom_bank_mode == 1)
				return prg_rom[addr + 0x2000 * rom_bank[6] - 0xC000];
			return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 2) - 0xC000];

		// CPU $E000-$FFFF: 8 KiB PRG ROM bank, fixed to the last bank
		case 0xE: case 0xF:
			return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 1) - 0xE000];

		default: return 0xFF;
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		switch (addr >> 12)
		{
		// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		case 0x6: case 0x7:
			if (properties.has_prg_ram && !prg_ram_write_protection)
				prg_ram[addr - 0x6000] = data;
			break;

		// CPU $8000-$9FFF; bank select (even), bank data (odd)
		case 0x8: case 0x9:
			if (addr & 0x01)
			{
				rom_bank[bank_reg_select] = data;
				if (bank_reg_select == 0 || bank_reg_select == 1)
					rom_bank[bank_reg_select] &= ~0x01;
				else if (bank_reg_select == 6 || bank_reg_select == 7)
					rom_bank[bank_reg_select] &= ~0xC0;
			}
			else
			{
				bank_reg_select = data;
				prg_rom_bank_mode = data & 0x40;
				chr_a12_inversion = data & 0x80;
			}
			break;

		// CPU $A000-$BFFF; mirroring (even), PRG RAM protect (odd)
		case 0xA: case 0xB:
			if (addr & 0x01)
			{
				prg_ram_write_protection = data & 0x40;
				prg_ram_enabled = data & 0x80;
			}
			else
			{
				nametable_mirroring = data & 0x01;
				// TODO This bit has no effect on cartridges with hardwired 4-screen VRAM
				// In the iNES and NES 2.0 formats, this can be identified through bit 3 of byte $06 of the header.
			}
			break;

		// CPU $C000-$DFFF; IRQ latch (even), IRQ reload (odd)
		case 0xC: case 0xD:
			if (addr & 0x01)
				IRQ_counter_reload = data;
			else
				; // TODO
			break;

		// CPU $E000-$FFFF: IRQ disable (even), IRQ enable (odd)
		case 0xE: case 0xF:
			IRQ_enabled = addr & 0x01;
			break;

		default: break;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		switch (addr / 0x400)
		{
		// PPU $0000-$07FF (or $1000-$17FF): 2 KiB switchable CHR bank
		case 0:
			if (!chr_a12_inversion)
				return chr[addr + 0x800 * rom_bank[0]];
			return chr[addr + 0x400 * rom_bank[2]];
		
		case 1:
			if (!chr_a12_inversion)
				return chr[addr + 0x800 * rom_bank[0]];
			return chr[addr + 0x400 * rom_bank[3] - 0x400];

		// PPU $0800-$0FFF (or $1800-$1FFF): 2 KiB switchable CHR bank
		case 2:
			if (!chr_a12_inversion)
				return chr[addr + 0x800 * rom_bank[1] - 0x800];
			return chr[addr + 0x400 * rom_bank[4] - 0x800];

		case 3:
			if (!chr_a12_inversion)
				return chr[addr + 0x800 * rom_bank[1] - 0x800];
			return chr[addr + 0x400 * rom_bank[5] - 0xC00];

		// PPU $1000-$13FF (or $0000-$03FF): 1 KiB switchable CHR bank
		case 4:
			if (!chr_a12_inversion)
				return chr[addr + 0x400 * rom_bank[2] - 0x1000];
			return chr[addr + 0x800 * rom_bank[0] - 0x1000];

		// PPU $1400-$17FF (or $0400-$07FF): 1 KiB switchable CHR bank
		case 5:
			if (!chr_a12_inversion)
				return chr[addr + 0x400 * rom_bank[3] - 0x1400];
			return chr[addr + 0x800 * rom_bank[0] - 0x1000];

		// PPU $1800-$1BFF (or $0800-$0BFF): 1 KiB switchable CHR bank
		case 6:
			if (!chr_a12_inversion)
				return chr[addr + 0x400 * rom_bank[4] - 0x1800];
			return chr[addr + 0x800 * rom_bank[1] - 0x1800];

		// PPU $1C00-$1FFF (or $0C00-$0FFF): 1 KiB switchable CHR bank
		case 7:
			if (!chr_a12_inversion)
				return chr[addr + 0x400 * rom_bank[5] - 0x1C00];
			return chr[addr + 0x800 * rom_bank[1] - 0x1800];

		default: return 0xFF; // impossible
		}
	}

	void WriteCHR(u16 addr, u8 data) override
	{
		if (properties.has_chr_ram)
			chr[addr] = data;
	}

	u16 GetNametableAddr(u16 addr) override
	{
		if (properties.mirroring == 0)
			return NametableAddrHorizontal(addr);
		return NametableAddrVertical(addr);
	};

private:
	bool nametable_mirroring;
	bool chr_a12_inversion;
	bool IRQ_enabled;
	bool prg_ram_enabled;
	bool prg_ram_write_protection;
	bool prg_rom_bank_mode;
	unsigned bank_reg_select : 3 = 0;
	u8 IRQ_counter_reload;
	u8 value_last_read_from_prg_ram = 0;
	u8 rom_bank[8]{}; // 0..5 : CHR.    6, 7 : PRG
};

