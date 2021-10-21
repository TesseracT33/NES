#pragma once

#include "BaseMapper.h"

class MMC1 final : public BaseMapper
{
public:
	MMC1(MapperProperties mapper_properties) : BaseMapper(mapper_properties) {}

	// TODO: how to distinguish between the different SxROM boards with CHR ram?

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x5FFF)
		{
			throw std::runtime_error(std::format("Invalid address ${:X} given as argument to MMC1::ReadPRG(u16).", addr));
		}
		// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		if (addr <= 0x7FFF)
		{
			if (properties.has_prg_ram)
				return prg_ram[addr - 0x6000];
			return 0xFF;
		}
		// CPU $8000-$BFFF and CPU $C000-FFFF: depends on the control register
		switch (control_reg >> 2 & 3)
		{
		case 0: case 1: // 32 KiB mode; $8000-$FFFF is mapped to a 32 KiB bank (bit 0 of the bank number is ignored).
			if (properties.prg_rom_size >= 0x8000)
				return prg_rom[addr - 0x8000 + (prg_bank & 0xE) * 0x8000];
			// If PRG ROM is smaller than 32 KiB (in that case 16 KiB), transform addresses $C000-$FFFF into $8000-$BFFF.
			return prg_rom[(addr & ~0x4000) - 0x8000];

		case 2: // 16 KiB mode 1; Fix the first bank at $8000-$BFFF and switch 16 KiB bank at $C000-$FFFF.
			if (addr <= 0xBFFF)
				return prg_rom[addr - 0x8000];
			return prg_rom[addr - 0xC000 + prg_bank * 0x4000];

		case 3: // 16 KiB mode 2; Switch 16 KiB bank at $8000-$BFFF and fix the last bank at $C000-$FFFF.
			if (addr <= 0xBFFF)
				return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
			return prg_rom[addr - 0xC000 + (properties.num_prg_rom_banks - 1) * 0x4000];

		default: return 0xFF; // impossible
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr <= 0x5FFF)
		{
			throw std::runtime_error(std::format("Invalid address ${:X} given as argument to MMC1::WritePRG(u16, u8).", addr));
		}
		else if (addr <= 0x7FFF)
		{
			if (properties.has_prg_ram)
				prg_ram[addr - 0x6000] = data;
		}
		else
		{
			// To change a registers value (control reg, chr bank etc), write five times with bit 7 clear and a bit of the desired value in bit 0. 
			// On the first four writes, bit 0 is shifted into a shift register.
			// On the fifth write, bit 0 and the shift register contents are copied into an internal register selected by bits 14 and 13 of the address, and then it clears the shift register.
			// The shift register can also be cleared by writing any value where bit 7 is set.
			if (data & 0x80)
			{
				shift_reg = 0x10;
				control_reg |= 0xC;
			}
			else
			{
				static unsigned times_written = 0;
				shift_reg = shift_reg >> 1 | data << 4;
				if (++times_written == 5)
				{
					switch (addr >> 12)
					{
					case 0x8: case 0x9:
						control_reg = shift_reg;
						break;

					case 0xA: case 0xB:
						if (properties.has_chr_ram)
						{
							chr_bank_0 = shift_reg & 1;
							prg_ram_bank = shift_reg >> 2;
						}
						else
							chr_bank_0 = shift_reg;
						break;

					case 0xC: case 0xD:
						if (properties.has_chr_ram)
						{
							chr_bank_1 = shift_reg & 1;
							prg_ram_bank = shift_reg >> 2;
						}
						else
							chr_bank_1 = shift_reg;
						break;

					case 0xE: case 0xF:
						prg_bank = shift_reg % properties.num_prg_rom_banks;
						break;

					default: break; // impossible
					}
					shift_reg = 0x10;
					times_written = 0;
				}
			}
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		// 4 KiB mode; $0000-$0FFF and $1000-$1FFF have two separate 4 KiB banks.
		if (control_reg & 0x10)
		{
			if (addr <= 0x0FFF)
				return chr[addr + 0x1000 * chr_bank_0];
			return chr[addr - 0x1000 + 0x1000 * chr_bank_1];
		}
		// 8 KB mode; $0000-$1FFF is mapped to a single 8 KiB bank (bit 0 of the bank number is ignored).
		return chr[addr + 0x2000 * (chr_bank_0 & 0x1E)];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		if (!properties.has_chr_ram)
			return;

		if (control_reg & 0x10)
		{
			if (addr <= 0x0FFF)
				chr[addr + 0x1000 * chr_bank_0] = data;
			else
				chr[addr - 0x1000 + 0x1000 * chr_bank_1] = data;
		}
		else
			chr[addr + 0x2000 * (chr_bank_0 & 0x1E)] = data;
	};

	u16 GetNametableAddr(u16 addr) override
	{
		switch (control_reg & 3)
		{
		case 0: return NametableAddrSingleLower(addr);
		case 1: return NametableAddrSingleUpper(addr);
		case 2: return NametableAddrVertical(addr);
		case 3: return NametableAddrHorizontal(addr);
		}
	};

private:
	// TODO: what are the default values of these?
	unsigned chr_bank_0   : 5 = 0;
	unsigned chr_bank_1   : 5 = 0;
	unsigned control_reg  : 5 = 0xC; /* nesdev seem to suggest that many carts start in PRG ROM bank mode 3. */
	unsigned prg_bank     : 4 = 0;
	unsigned prg_ram_bank : 2 = 0;
	unsigned shift_reg    : 5 = 0x10;
};