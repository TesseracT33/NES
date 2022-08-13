export module PPU;

import NumericalTypes;
import SerializationStream;

import <algorithm>;
import <array>;
import <bit>;
import <format>;
import <limits>;
import <vector>;

namespace PPU
{
	export
	{
		uint GetFrameBufferSize();
		u8 PeekOAMDMA();
		u8 PeekRegister(u16 addr);
		void PowerOn();
		u8 ReadOAMDMA();
		u8 ReadRegister(u16 addr);
		void Reset();
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteOAMDMA(u8 data);
		void WriteRegister(u16 addr, u8 data);
	}

	enum class TileType { 
		BG, OBJ
	};

	struct RGB
	{
		u8 r, g, b;
	};

	template<TileType>
	u8 GetNESColorFromColorID(u8 col_id, u8 palette_id);

	void CheckNMI();
	bool InVblank();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
	void PushPixelToFramebuffer(u8 nes_col);
	u8 ReadMemory(u16 addr);
	u8 ReadPaletteRAM(u16 addr);
	void ReloadBackgroundShiftRegisters();
	void ReloadSpriteShiftRegisters(uint sprite_index);
	void SetA12(bool new_val);
	void ShiftPixel();
	void StepCycle();
	void UpdateBGTileFetching();
	void UpdateSpriteEvaluation();
	void UpdateSpriteTileFetching();
	void WriteMemory(u16 addr, u8 data);
	void WritePaletteRAM(u16 addr, u8 data);

	// PPU IO open bus related. See https://wiki.nesdev.org/w/index.php?title=PPU_registers#Ports
	// and the 'NES PPU Open-Bus Test' test rom readme
	struct OpenBusIO
	{
		OpenBusIO() { decayed.fill(true); }

		u8 Read(u8 mask = 0xFF);
		void Write(u8 data);
		void UpdateValue(u8 data, u8 mask);
		void UpdateDecay(uint elapsed_ppu_cycles);
		void UpdateDecayOnIOAccess(u8 mask);

		static constexpr uint decay_ppu_cycle_length = 262 * 341 * 36; // roughly 600 ms = 36 frames; how long it takes for a bit to decay to 0.
		u8 value = 0; // the value read back when reading from open bus.
		std::array<bool, 8> decayed{}; // each bit can decay separately
		std::array<uint, 8> ppu_cycles_since_refresh{};
	} open_bus_io;

	struct ScrollRegisters
	{
		void IncrementCoarseX();
		void IncrementFineY();

		/* Composition of 'v' (and 't'):
		  yyy NN YYYYY XXXXX
		  ||| || ||||| +++++-- coarse X scroll
		  ||| || +++++-------- coarse Y scroll
		  ||| ++-------------- nametable select
		  +++----------------- fine Y scroll
		*/
		uint v : 15; // Current VRAM address (15 bits): yyy NN YYYYY XXXXX
		uint t : 15; // Temporary VRAM address (15 bits); can also be thought of as the address of the top left onscreen tile.
		uint x : 3; // Fine X scroll (3 bits)
		bool w; // First or second $2005/$2006 write toggle (1 bit)
	} scroll;

	struct SpriteEvaluation
	{
		void IncrementByteIndex();
		void IncrementSpriteIndex();
		void Reset();
		void Restart();

		uint num_sprites_copied = 0; // (0-8) the number of sprites copied from OAM into secondary OAM
		uint sprite_index; // (0-63) index of the sprite currently being checked in OAM
		uint byte_index; // (0-3) byte of this sprite
		bool idle = false; // whether the sprite evaluation is finished for the current scanline
		// Whether the 0th byte was copied from OAM into secondary OAM
		bool sprite_0_included_current_scanline = false;
		// Sprite evaluation is done for the *next* scanline. Set this to true during sprite evaluation, and then copy 'next' into 'current' when transitioning to a new scanline.
		bool sprite_0_included_next_scanline = false;
	} sprite_evaluation;

	struct TileFetcher
	{
		void StartOver();

		// fetched data of the tile currently being fetched
		u8 tile_num; // nametable byte; hex digits 2-1 of the address of the tile's pattern table entries. 
		u8 attribute_table_byte; // palette data for the tile. depending on which quadrant of a 16x16 pixel metatile this tile is in, two bits of this byte indicate the palette number (0-3) used for the tile
		u8 pattern_table_tile_low, pattern_table_tile_high; // actual colour data describing the tile. If bit n of tile_high is 'x' and bit n of tile_low is 'y', the colour id for pixel n of the tile is 'xy'

		// used only for background tiles
		u8 attribute_table_quadrant;

		// used only for sprites
		u8 sprite_y_pos;
		u8 sprite_attr;

		u16 addr;

		uint cycle_step : 3; // (0-7)
	} tile_fetcher;

	constexpr int pre_render_scanline = -1;
	constexpr uint num_colour_channels = 3;
	constexpr uint num_pixels_per_scanline = 256; // Horizontal resolution

	/* "A12" refers to the 12th ppu address bus pin.
	   It is set/cleared by the PPU during rendering, specifically when fetching BG tiles / sprites.
	   It can also be set/cleared outside of rendering, when $2006/$2007 is read/written to,
	   for the reason that the address bus pins outside of rendering are set to the vram address (scroll.v).
	   MMC3 contains a scanline counter that gets clocked when A12 (0 -> 1), once A12 has remained low for 3 cpu cycles.
	   TODO: in the future: consider the entire address bus, not just A12? This is basically just to get MMC3 to work. */
	bool a12;
	bool cycle_340_was_skipped_on_last_scanline; // On NTSC, cycle 340 of the pre render scanline may be skipped every other frame.
	bool nmi_line;
	bool odd_frame;
	bool rendering_is_enabled; /* == ppumask.bg_enable || ppumask.sprite_enable */
	bool set_sprite_0_hit_flag;

	u8 pixel_x_pos;

	struct
	{
		u8 nametable_select : 2; /* Base nametable address (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00) */
		u8 incr_mode : 1; /* VRAM address increment per CPU read/write of PPUDATA (0: add 1, going across; 1: add 32, going down) */
		u8 sprite_tile_select : 1; /* Sprite pattern table address for 8x8 sprites (0: $0000; 1: $1000; ignored in 8x16 mode) */
		u8 bg_tile_select : 1; /* Background pattern table address (0: $0000; 1: $1000) */
		u8 sprite_height : 1; /* Sprite size (0: 8x8 pixels; 1: 8x16 pixels) (0: read backdrop from EXT pins; 1: output color on EXT pins) */
		u8 ppu_master : 1; /* PPU master/slave select */
		u8 nmi_enable : 1; /* Generate an NMI at the start of the vertical blanking interval (0: off; 1: on) */
	} ppuctrl;

	struct
	{
		u8 greyscale : 1; /* 0: normal color, 1: produce a greyscale display */
		u8 bg_left_col_enable : 1; /* 1: Show background in leftmost 8 pixels of screen, 0: Hide */
		u8 sprite_left_col_enable : 1; /* 1: Show sprites in leftmost 8 pixels of screen, 0: Hide */
		u8 bg_enable : 1; /* 1: Show background */
		u8 sprite_enable : 1; /*  1: Show sprites */
		u8 emphasize_red : 1; /* Emphasize red (green on PAL/Dendy) */
		u8 emphasize_green : 1; /* Emphasize green (red on PAL/Dendy) */
		u8 emphasize_blue : 1; /* Emphasize blue */
	} ppumask;

	struct
	{
		/* Least significant bits previously written into a PPU register
			(due to register not being updated for this address) */
		u8 : 5;
		/* Sprite overflow. The intent was for this flag to be set
			whenever more than eight sprites appear on a scanline, but a
			hardware bug causes the actual behavior to be more complicated
			and generate false positives as well as false negatives; see
			PPU sprite evaluation. This flag is set during sprite
			evaluation and cleared at dot 1 (the second dot) of the
			pre-render line. */
		u8 sprite_overflow : 1;
		/* Sprite 0 Hit.  Set when a nonzero pixel of sprite 0 overlaps
			a nonzero background pixel; cleared at dot 1 of the pre-render
			line. Used for raster timing. */
		u8 sprite_0_hit : 1;
		/* Vertical blank has started (0: not in vblank; 1: in vblank).
			Set at dot 1 of line 241 (the line *after* the post-render
			line); cleared after reading $2002 and at dot 1 of the
			pre-render line. */
		u8 vblank : 1;
	} ppustatus;

	u8 ppuscroll;
	u8 ppudata;
	u8 oamaddr;
	u8 oamdma;

	u8 oamaddr_at_cycle_65;

	int scanline;

	uint cpu_cycle_counter; /* Used in PAL mode to sync ppu to cpu */
	uint cpu_cycles_since_a12_set_low = 0;
	uint framebuffer_pos;
	uint scanline_cycle;
	uint secondary_oam_sprite_index /* (0-7) index of the sprite currently being fetched (ppu dots 257-320). */;

	std::array<u8, 0x100 > oam; /* Not mapped. Holds sprite data (four bytes each for up to 64 sprites). */
	std::array<u8, 0x20  > palette_ram; /* Mapped to PPU $3F00-$3F1F (mirrored at $3F20-$3FFF). */
	std::array<u8, 0x20  > secondary_oam; /* Holds sprite data for sprites to be rendered on the next scanline. */

	std::array<u8, 8> sprite_attribute_latch;
	std::array<u8, 16> sprite_pattern_shift_reg;
	std::array<u16, 2> bg_palette_attr_reg; // These are actually 8 bits on real HW, but it's easier this way. Similar to the pattern shift registers, the MSB contain data for the current tile, and the bottom LSB for the next tile.
	std::array<u16, 2> bg_pattern_shift_reg;

	std::array<int, 8> sprite_x_pos_counter;

	std::vector<u8> framebuffer;
};