#pragma once

#include "BaseMapper.h"

class MMC1 : public BaseMapper
{
public:
	MMC1(const std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, MutateProperties(properties)) {}

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
			return prg_ram[addr - 0x6000];
		}
		// CPU $8000-$BFFF and CPU $C000-FFFF: depends on the control register
		switch (prg_rom_bank_mode)
		{
		case 0: case 1: // 32 KiB mode; $8000-$FFFF is mapped to a 32 KiB bank (bit 0 of the bank number is ignored).
			if (properties.prg_rom_size < 0x8000)
			{
				// If PRG ROM is smaller than 32 KiB (in that case 16 KiB), transform addresses $C000-$FFFF into $8000-$BFFF (and in turn into $0000-$3FFF).
				return prg_rom[addr & ~0xC000];
			}
			else
			{
				// Effectively, $8000-$BFFF is mapped to 'prg_bank & ~0x01', and $C000-$FFFF to '(prg_bank & ~0x01) + 1'
				// If 'prg_bank & ~0x01' is the last 16 KiB bank, transform addresses $C000-$FFFF into $8000-$BFFF.
				u8 aligned_bank = prg_bank & ~0x01;
				if (aligned_bank == properties.num_prg_rom_banks - 1)
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
			return prg_rom[addr - 0xC000 + (properties.num_prg_rom_banks - 1) * 0x4000];
		}
		return 0xFF;
	};

	void WritePRG(u16 addr, u8 data) override
	{
		if (addr <= 0x5FFF)
		{
			return;
		}
		else if (addr <= 0x7FFF)
		{
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
				prg_rom_bank_mode = 3; /* Mesen source code (MMC1.h): the control register should be reset as well. */
			}
			else
			{
				shift_reg = shift_reg >> 1 | data << 4;
				if (++times_written == 5)
				{
					switch (addr >> 12)
					{
					case 0x8: case 0x9: /* Control (internal, $8000-$9FFF) */
						chr_mirroring = shift_reg;
						prg_rom_bank_mode = shift_reg >> 2;
						chr_bank_mode = shift_reg & 0x10;
						break;

					case 0xA: case 0xB: /* CHR bank 0 (internal, $A000-$BFFF) */
						chr_bank_0 = shift_reg % properties.num_chr_banks;
						break;

					case 0xC: case 0xD: /* CHR bank 1 (internal, $C000-$DFFF) */
						chr_bank_1 = shift_reg % properties.num_chr_banks;
						break;

					case 0xE: case 0xF: /* PRG bank (internal, $E000-$FFFF) */
						prg_bank = (shift_reg & 0xF) % properties.num_prg_rom_banks;
						prg_ram_enabled = !(shift_reg & 0x10);
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
		// 8 KiB mode; $0000-$1FFF is mapped to a single 8 KiB bank (bit 0 of the bank number is ignored).
		// Effectively, this is mapping $0000-$0FFF to 'chr_bank_0 & ~0x01', and $1000-$1FFF to '(chr_bank_0 & ~0x01) + 1'
		// If 'chr_bank_0 & ~0x01' is the last 4 KiB bank, transform addresses in $1000-$1FFF into $0000-$0FFF
		if (chr_bank_mode == 0)
		{
			u8 aligned_bank = chr_bank_0 & ~0x01;
			if (aligned_bank == properties.num_chr_banks - 1)
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
		{
			u8 aligned_bank = chr_bank_0 & ~0x01;
			if (aligned_bank == properties.num_chr_banks - 1)
				addr &= 0xFFF;
			chr[addr + 0x1000 * aligned_bank] = data;
		}
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
		default: return addr; // impossible
		}
	};

protected:
	// TODO: what are the default values of these?
	bool prg_ram_enabled = false;
	bool chr_bank_mode = 0;
	unsigned chr_bank_0 : 5 = 0;
	unsigned chr_bank_1 : 5 = 0;
	unsigned chr_mirroring : 2 = 0;
	unsigned prg_bank : 4 = 0;
	unsigned prg_ram_bank : 2 = 0;
	unsigned prg_rom_bank_mode : 2 = 3; /* nesdev seem to suggest that many carts start in PRG ROM bank mode 3. */
	unsigned shift_reg : 5 = 0x10;

private:
	static MapperProperties MutateProperties(MapperProperties properties)
	{
		SetCHRBankSize(properties, 0x1000);
		SetCHRRAMSize(properties, 0x2000); /* Known carts with CHR RAM have 8 KiB capacity. */
		SetPRGRAMSize(properties, 0x2000); /* TODO: some carts have 32 KiB */
		return properties;
	};
};