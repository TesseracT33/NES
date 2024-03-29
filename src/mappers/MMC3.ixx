export module MMC3;

import BaseMapper;
import CPU;
import MapperProperties;

import NumericalTypes;
import SerializationStream;

import <array>;
import <vector>;

export class MMC3 : public BaseMapper
{
public:
	MMC3(std::vector<u8> chr_prg_rom, MapperProperties properties) :
		BaseMapper(chr_prg_rom, MutateProperties(properties)) {}

	u8 ReadPRG(u16 addr) override
	{
		switch (addr >> 12) {
			// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		case 0x6: case 0x7:
			return prg_ram[addr - 0x6000];

			// CPU $8000-$9FFF (or $C000-$DFFF): 8 KiB switchable PRG ROM bank
		case 0x8: case 0x9:
			if (prg_rom_bank_mode == 0) {
				return prg_rom[addr + 0x2000 * rom_bank[6] - 0x8000];
			}
			else {
				return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 2) - 0x8000];
			}

			// CPU $A000-$BFFF: 8 KiB switchable PRG ROM bank
		case 0xA: case 0xB:
			return prg_rom[addr + 0x2000 * rom_bank[7] - 0xA000];

			// CPU $C000-$DFFF (or $8000-$9FFF): 8 KiB PRG ROM bank, fixed to the second-last bank
		case 0xC: case 0xD:
			if (prg_rom_bank_mode == 1) {
				return prg_rom[addr + 0x2000 * rom_bank[6] - 0xC000];
			}
			else {
				return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 2) - 0xC000];
			}

			// CPU $E000-$FFFF: 8 KiB PRG ROM bank, fixed to the last bank
		case 0xE: case 0xF:
			return prg_rom[addr + 0x2000 * (properties.num_prg_rom_banks - 1) - 0xE000];

		default:
			return 0xFF;
		}
	};

	void WritePRG(u16 addr, u8 data) override
	{
		switch (addr >> 12) {
			// CPU $6000-$7FFF: 8 KiB PRG RAM bank (optional)
		case 0x6: case 0x7:
			prg_ram[addr - 0x6000] = data;
			break;

			// CPU $8000-$9FFF; bank select (even), bank data (odd)
		case 0x8: case 0x9:
			if (addr & 1) {
				rom_bank[bank_reg_select] = data;
				/* MMC3 has 8 CHR address lines, and the CHR capacity is always 256K, so no need to mod with the number of available banks, if a CHR bank was chosen.
				   R0 and R1 ignore the bottom bit, as the value written still counts banks in 1KB units but odd numbered banks can't be selected. */
				if (bank_reg_select == 0 || bank_reg_select == 1) {
					rom_bank[bank_reg_select] &= 0xFE;
				}
				/* R6 and R7 (PRG) will ignore the top two bits, as MMC3 has only 6 PRG ROM address lines. */
				else if (bank_reg_select == 6 || bank_reg_select == 7) {
					rom_bank[bank_reg_select] &= 0x3F;
				}
			}
			else {
				bank_reg_select = data;
				prg_rom_bank_mode = data & 0x40;
				chr_a12_inversion = data & 0x80;
			}
			break;

			// CPU $A000-$BFFF; mirroring (even), PRG RAM protect (odd)
		case 0xA: case 0xB:
			if (addr & 1) {
				prg_ram_write_protection = data & 0x40; // (0: allow writes; 1: deny writes)
				prg_ram_enabled = data & 0x80;
			}
			else {
				nametable_mirroring = data & 0x01; // (0: vertical; 1: horizontal)
			}
			break;

			// CPU $C000-$DFFF; IRQ latch (even), IRQ reload (odd)
		case 0xC: case 0xD:
			if (addr & 1) {
				irq_counter = 0;
				reload_irq_counter_on_next_clock = true;
			}
			else {
				irq_counter_reload = data;
			}
			break;

			// CPU $E000-$FFFF: IRQ disable (even), IRQ enable (odd)
		case 0xE: case 0xF:
			irq_enabled = addr & 1;
			if (!irq_enabled) {
				CPU::SetIrqHigh(CPU::IrqSource::MMC3);
			}
			// TODO Writing any value to this register will disable MMC3 interrupts AND acknowledge any pending interrupts.
			break;

		default:
			break;
		}
	};

	u8 ReadCHR(u16 addr) override
	{
		auto physical_addr = GetPhysicalCHRAddress(addr);
		return chr[physical_addr];
	}

	void WriteCHR(u16 addr, u8 data) override
	{
		if (!properties.has_chr_ram) {
			return;
		}
		auto physical_addr = GetPhysicalCHRAddress(addr);
		chr[physical_addr] = data;
	}

	const std::array<int, 4>& GetNametableMap() const override
	{
		/* $A000.0 is ignored on cartridges with hardwired 4-screen VRAM. */
		/* TODO: put this in BaseMapper? */
		if (properties.hard_wired_four_screen) {
			return nametable_map_fourscreen;
		}
		else if (nametable_mirroring == 0) {
			return nametable_map_vertical;
		}
		else {
			return nametable_map_horizontal;
		}
	};

	virtual void ClockIRQ() override
	{
		if (irq_counter == 0 || reload_irq_counter_on_next_clock) {
			irq_counter = irq_counter_reload;
			reload_irq_counter_on_next_clock = false;
		}
		else {
			irq_counter--;
		}
		if (irq_counter == 0 && irq_enabled) {
			CPU::SetIrqLow(CPU::IrqSource::MMC3);
		}
	}

	virtual void StreamState(SerializationStream& stream) override
	{
		BaseMapper::StreamState(stream);

		stream.StreamPrimitive(nametable_mirroring);
		stream.StreamPrimitive(chr_a12_inversion);
		stream.StreamPrimitive(irq_enabled);
		stream.StreamPrimitive(prg_ram_enabled);
		stream.StreamPrimitive(prg_ram_write_protection);
		stream.StreamPrimitive(prg_rom_bank_mode);
		stream.StreamPrimitive(reload_irq_counter_on_next_clock);

		bank_reg_select = stream.StreamBitfield(bank_reg_select);

		stream.StreamPrimitive(irq_counter);
		stream.StreamPrimitive(irq_counter_reload);
		stream.StreamPrimitive(prg_ram_open_bus);

		stream.StreamArray(rom_bank);
	};

protected:
	bool nametable_mirroring = 0;

	/* 0: two 2 KB banks at $0000-$0FFF, four 1 KB banks at $1000-$1FFF;
	   1: two 2 KB banks at $1000-$1FFF, four 1 KB banks at $0000-$0FFF. */
	bool chr_a12_inversion = 0;

	bool irq_enabled = false;
	bool prg_ram_enabled = false;
	bool prg_ram_write_protection = false;

	/* 0: $8000-$9FFF swappable, $C000-$DFFF fixed to second-last bank;
	   1: $C000-$DFFF swappable, $8000-$9FFF fixed to second-last bank. */
	bool prg_rom_bank_mode = 0;

	bool reload_irq_counter_on_next_clock = false;

	/* Specifies which bank register to update on next write to Bank Data register ($8001-$9FFF, odd)
	   000: R0: Select 2 KB CHR bank at PPU $0000-$07FF (or $1000-$17FF)
	   001: R1: Select 2 KB CHR bank at PPU $0800-$0FFF (or $1800-$1FFF)
	   010: R2: Select 1 KB CHR bank at PPU $1000-$13FF (or $0000-$03FF)
	   011: R3: Select 1 KB CHR bank at PPU $1400-$17FF (or $0400-$07FF)
	   100: R4: Select 1 KB CHR bank at PPU $1800-$1BFF (or $0800-$0BFF)
	   101: R5: Select 1 KB CHR bank at PPU $1C00-$1FFF (or $0C00-$0FFF)
	   110: R6: Select 8 KB PRG ROM bank at $8000-$9FFF (or $C000-$DFFF)
	   111: R7: Select 8 KB PRG ROM bank at $A000-$BFFF */
	uint bank_reg_select : 3 = 0;

	u8 irq_counter = 0;
	u8 irq_counter_reload = 0;
	u8 prg_ram_open_bus = 0;
	std::array<u8, 8> rom_bank{}; // 0..5 : CHR; 6, 7 : PRG

	std::size_t GetPhysicalCHRAddress(const u16 addr) const
	{
		/* CHR map mode -> $8000.D7 = 0  $8000.D7 = 1
		   PPU Bank	         Value of MMC3 register
		   $0000-$03FF	        R0          R2
		   $0400-$07FF	        R0          R3
		   $0800-$0BFF	        R1          R4
		   $0C00-$0FFF	        R1          R5
		   $1000-$13FF	        R2          R0
		   $1400-$17FF	        R3          R0
		   $1800-$1BFF	        R4          R1
		   $1C00-$1FFF	        R5          R1

		   CHR inversion:
		   0: two 2 KB banks at $0000-$0FFF, four 1 KB banks at $1000-$1FFF;
		   1: two 2 KB banks at $1000-$1FFF, four 1 KB banks at $0000-$0FFF.
		*/
		switch (addr >> 8 & 0x1F) {
			// PPU $0000-$07FF (or $1000-$17FF): 2 KiB switchable CHR bank
		case 0x00: case 0x01: case 0x02: case 0x03:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[2]
				: addr + 0x400 * rom_bank[0];

		case 0x04: case 0x05: case 0x06: case 0x07:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[3] - 0x400
				: addr + 0x400 * rom_bank[0];

			// PPU $0800-$0FFF (or $1800-$1FFF): 2 KiB switchable CHR bank
		case 0x08: case 0x09: case 0x0A: case 0x0B:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[4] - 0x800
				: addr + 0x400 * rom_bank[1] - 0x800;

		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[5] - 0xC00
				: addr + 0x400 * rom_bank[1] - 0x800;

			// PPU $1000-$13FF (or $0000-$03FF): 1 KiB switchable CHR bank
		case 0x10: case 0x11: case 0x12: case 0x13:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[0] - 0x1000
				: addr + 0x400 * rom_bank[2] - 0x1000;

			// PPU $1400-$17FF (or $0400-$07FF): 1 KiB switchable CHR bank
		case 0x14: case 0x15: case 0x16: case 0x17:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[0] - 0x1000
				: addr + 0x400 * rom_bank[3] - 0x1400;

			// PPU $1800-$1BFF (or $0800-$0BFF): 1 KiB switchable CHR bank
		case 0x18: case 0x19: case 0x1A: case 0x1B:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[1] - 0x1800
				: addr + 0x400 * rom_bank[4] - 0x1800;

			// PPU $1C00-$1FFF (or $0C00-$0FFF): 1 KiB switchable CHR bank
		case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			return chr_a12_inversion
				? addr + 0x400 * rom_bank[1] - 0x1800
				: addr + 0x400 * rom_bank[5] - 0x1C00;

		default: /* Should never happen */
			std::unreachable();
		}
	}

private:
	static MapperProperties MutateProperties(MapperProperties properties)
	{
		SetCHRRAMSize(properties, 0x2000);
		SetPRGRAMSize(properties, 0x2000);
		SetPRGROMBankSize(properties, 0x2000);
		return properties;
	}
};