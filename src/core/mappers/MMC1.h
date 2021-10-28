#pragma once

#include "BaseMapper.h"

class MMC1 : public BaseMapper
{
public:
	MMC1(MapperProperties properties) : BaseMapper(properties),
		/* If CHR is RAM, then 'chr_size' will be given as 0. However, all carts should then have 128 KiB of CHR RAM. */
		num_chr_banks((properties.chr_size > 0 ? properties.chr_size : chr_ram_size) / chr_bank_size),
		num_prg_rom_banks(properties.prg_rom_size / prg_rom_bank_size)
	{
		if (properties.chr_size == 0)
			properties.chr_size = chr_ram_size;
	}

	// TODO: how to distinguish between the different SxROM boards with CHR ram?
	// TODO: implement PRG RAM banking

	u8 ReadPRG(u16 addr) override
	{
		if (addr <= 0x5FFF)
		{
			return 0xFF;
		}
		// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		if (addr <= 0x7FFF)
		{
			if (properties.has_prg_ram)
				return prg_ram[addr - 0x6000];
			return 0xFF;
		}
		// CPU $8000-$BFFF and CPU $C000-FFFF: depends on the control register
		switch (prg_rom_bank_mode)
		{
		case 0: case 1: // 32 KiB mode; $8000-$FFFF is mapped to a 32 KiB bank (bit 0 of the bank number is ignored).
			if (properties.prg_rom_size < 0x8000)
			{
				// If PRG ROM is smaller than 32 KiB (in that case 16 KiB), transform addresses $C000-$FFFF into $8000-$BFFF.
				return prg_rom[(addr & ~0x4000) - 0x8000];
			}
			else
			{
				// Effectively, $8000-$BFFF is mapped to 'prg_bank & ~0x01', and $C000-$FFFF to '(prg_bank & ~0x01) + 1'
				// If 'prg_bank & ~0x01' is the last 16 KiB bank, transform addresses $C000-$FFFF into $8000-$BFFF.
				u8 aligned_bank = prg_bank & ~0x01;
				if (aligned_bank == num_prg_rom_banks - 1)
					addr &= ~0x4000;
				return prg_rom[addr - 0x8000 + aligned_bank * 0x4000];
			}

		case 2: // 16 KiB mode 1; Fix the first bank at $8000-$BFFF and switch 16 KiB bank at $C000-$FFFF.
			if (addr <= 0xBFFF)
				return prg_rom[addr - 0x8000];
			return prg_rom[addr - 0xC000 + prg_bank * 0x4000];

		case 3: // 16 KiB mode 2; Switch 16 KiB bank at $8000-$BFFF and fix the last bank at $C000-$FFFF.
			if (addr <= 0xBFFF)
				return prg_rom[addr - 0x8000 + prg_bank * 0x4000];
			return prg_rom[addr - 0xC000 + (num_prg_rom_banks - 1) * 0x4000];
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr <= 0x5FFF)
		{
			return;
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
			// The shift register can also be reset to its original state ($10) by writing any value where bit 7 is set.
			// TODO: When the CPU writes to the serial port on consecutive cycles, the MMC1 ignores all writes but the first. 
			static int times_written = 0;
			if (data & 0x80)
			{
				shift_reg = 0x10;
				times_written = 0;
			}
			else
			{
				shift_reg >>= 1;
				shift_reg |= (data & 1) << 4;
				if (++times_written == 5)
				{
					switch (addr >> 12)
					{
					case 0x8: case 0x9: /* Control (internal, $8000-$9FFF) */
						chr_mirroring = shift_reg;
						prg_rom_bank_mode = shift_reg >> 2;
						chr_bank_mode = shift_reg >> 4;
						break;

					case 0xA: case 0xB: /* CHR bank 0 (internal, $A000-$BFFF) */
						chr_bank_0 = shift_reg % num_chr_banks;
						break;

					case 0xC: case 0xD: /* CHR bank 1 (internal, $C000-$DFFF) */
						chr_bank_1 = shift_reg % num_chr_banks;
						break;

					case 0xE: case 0xF: /* PRG bank (internal, $E000-$FFFF) */
						prg_bank = (shift_reg & 0xF) % num_prg_rom_banks;
						prg_ram_enabled = shift_reg & 0x10;
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
		// 8 KB mode; $0000-$1FFF is mapped to a single 8 KiB bank (bit 0 of the bank number is ignored).
		// Effectively, this is mapping $0000-$0FFF to 'chr_bank_0 & ~0x01', and $1000-$1FFF to '(chr_bank_0 & ~0x01) + 1'
		// If 'chr_bank_0 & ~0x01' is the last 4 KiB bank, transform addresses in $1000-$1FFF into $0000-$0FFF
		if (chr_bank_mode == 0)
		{
			u8 aligned_bank = chr_bank_0 & ~0x01;
			if (aligned_bank == num_chr_banks - 1)
				addr &= 0xFFF;
			return chr[addr + 0x1000 * aligned_bank];
		}
		// 4 KiB mode; $0000-$0FFF and $1000-$1FFF are mapped to separate 4 KiB banks.
		if (addr <= 0x0FFF)
			return chr[addr + 0x1000 * chr_bank_0];
		return chr[addr - 0x1000 + 0x1000 * chr_bank_1];
	};

	void WriteCHR(u16 addr, u8 data) override
	{
		if (!properties.has_chr_ram)
			return;

		if (chr_bank_mode == 0)
			chr[addr + 0x2000 * (chr_bank_0 & ~0x01)] = data;
		else
		{
			if (addr <= 0x0FFF)
				chr[addr + 0x1000 * chr_bank_0] = data;
			else
				chr[addr - 0x1000 + 0x1000 * chr_bank_1] = data;
		}
	};

	u16 TransformNametableAddr(u16 addr) override
	{
		switch (chr_mirroring)
		{
		case 0: return NametableAddrSingleLower(addr);
		case 1: return NametableAddrSingleUpper(addr);
		case 2: return NametableAddrVertical(addr);
		case 3: return NametableAddrHorizontal(addr);
		}
	};

protected:
	static const size_t chr_bank_size     = 0x1000;
	static const size_t chr_ram_size      = 0x32000;
	static const size_t prg_rom_bank_size = 0x4000;
	const size_t num_chr_banks;
	const size_t num_prg_rom_banks;

	// TODO: what are the default values of these?
	unsigned chr_bank_0        : 5 = 0;
	unsigned chr_bank_1        : 5 = 0;
	unsigned chr_mirroring     : 2 = 0;
	unsigned chr_bank_mode     : 1 = 0;
	unsigned prg_bank          : 4 = 0;
	unsigned prg_ram_bank      : 2 = 0;
	unsigned prg_ram_enabled   : 1 = 0;
	unsigned prg_rom_bank_mode : 2 = 3; /* nesdev seem to suggest that many carts start in PRG ROM bank mode 3. */
	unsigned shift_reg         : 5 = 0x10;
};