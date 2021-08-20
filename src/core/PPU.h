#pragma once

#include <stdexcept>
#include <limits>

#include "../Types.h"

#include "Bus.h"
#include "Component.h"
#include "CPU.h"

class PPU final : public Component
{
public:
	Bus* bus;
	CPU* cpu;

	void Initialize();
	void Update();

	// writing and reading done by the CPU to/from the registers at $2000-$2007, $4014
	u8 ReadFromPPUReg(u16 addr);
	void WriteToPPUReg(u16 addr, u8 data);

	inline bool IsInVblank() { return current_scanline >= 241; };

private:
	static const unsigned resolution_y = 240;

	struct Memory
	{
		u8 vram[0x1000]{};
		u8 palette_ram[0x20]{};
		u8 oam[0x100]{};

		// reading and writing done internally by the ppu, e.g. when PPUDATA is written/read to/from
		u8 Read(u16 addr) const
		{
			return u8();
		}

		void Write(u16 addr, u8 data)
		{

		}
	} memory;

	struct TileFetcher
	{
		u8 nametable_byte, attribute_table_byte, pattern_table_tile_low, pattern_table_tile_high;

		enum Step { fetch_nametable_byte, fetch_attribute_table_byte, fetch_pattern_table_tile_low, fetch_pattern_table_tile_high } step;
		bool sleep_on_next_update = true;
		u8 x_pos = 0; // index of the tile currently being fetched. Between 0-31, making 32 * 8 = 256 pixels

		enum class TileType { BG, OAM_8x8, OAM_8x16 } tile_type;
	} tile_fetcher;

	// PPU registers accessible by the CPU
	u8 PPUCTRL;
	u8 PPUMASK;
	u8 PPUSTATUS;
	u8 PPUSCROLL;
	u8 PPUADDR;
	u8 PPUDATA;
	u8 OAMADDR;
	u8 OAMDATA;
	u8 OAMDMA;

	u8 scroll_x;
	s16 scroll_y;

	u8 ppuaddr_first_byte_written;
	u16 ppuaddr_written_addr;

	bool NMI_occured, NMI_output;
	bool ppuscroll_written_to, ppuaddr_written_to;

	int current_scanline; // includes -1 (pre-render scanline) and 0-239 (visible scanlines) and 240 (post-render scanline)
	bool odd_frame; // during odd-numbered frames, the pre-render scanline lasts for 339 ppu cycles instead of 340 as normally

	unsigned ppu_cycle_counter;

	u8 secondary_OAM[32];

	bool reload_bg_shifters;
	u16 bg_pattern_table_shift_reg[2];
	u8 bg_palette_attr_shift_reg[2];

	struct Regs
	{
		u16 v; // Current VRAM address (15 bits)
		u16 t; // Temporary VRAM address (15 bits); can also be thought of as the address of the top left onscreen tile.
		u8 x; // Fine X scroll (3 bits)
		bool w; // First or second write toggle (1 bit)
	} regs;

	void ShiftPixel();
	void DoSpriteEvaluation();
	void UpdateTileFetcher();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
};

