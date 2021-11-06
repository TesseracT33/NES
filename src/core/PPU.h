#pragma once

#include "SDL.h"

#include <array>
#include <format>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../Observer.h"
#include "../Types.h"

#include "../gui/UserMessage.h"

#include "../debug/Logging.h"

#include "Bus.h"
#include "Component.h"
#include "CPU.h"
#include "System.h"

#include "mappers/BaseMapper.h"

class PPU final : public Component
{
public:
	using Component::Component;
	~PPU();

	Observer* gui;

	unsigned GetWindowScale() const { return window_scale; }
	unsigned GetWindowHeight() const { return standard.num_visible_scanlines * window_scale; }
	unsigned GetWindowWidth() const { return num_pixels_per_scanline * window_scale; }
	/* Note: vblank only begins after the "post-render" scanlines, i.e. on the same scanline as NMI is triggered. */
	bool IsInVblank() const { return scanline >= standard.nmi_scanline; }

	[[nodiscard]] bool CreateRenderer(const void* window_handle);
	void PowerOn(const System::VideoStandard standard);
	void Reset();
	void Update();

	// writing and reading done by the CPU to/from the registers at $2000-$2007, $4014
	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

	void SetWindowScale(unsigned scale) { this->window_scale = scale; }
	void SetWindowSize(unsigned width, unsigned height);

	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

private:
	enum class TileType { BG, OBJ };

	/* PPU operation details that are affected by the video standard (NTSC/PAL/Dendy): */
	struct Standard
	{
		bool oam_can_be_written_to_during_forced_blanking;
		bool pre_render_line_is_one_dot_shorter_on_every_other_frame;
		float dots_per_cpu_cycle;
		int nmi_scanline;
		int num_scanlines;
		int num_scanlines_per_vblank;
		int num_visible_scanlines;
	} standard = NTSC;

	const Standard NTSC = { true,  true, 3.0f, 241, 262, 20, 240 };
	const Standard PAL = { false, false, 3.2f, 240, 312, 70, 239 };
	const Standard Dendy = { true, false, 3.0f, 290, 312, 20, 239 };

	const int pre_render_scanline = -1;
	const int num_colour_channels = 3;
	const int num_cycles_per_scanline = 341; // On NTSC: is actually 340 on the pre-render scanline if on an odd-numbered frame
	const int num_pixels_per_scanline = 256; // Horizontal resolution

	const unsigned default_window_scale = 3;

	// https://wiki.nesdev.com/w/index.php?title=PPU_palettes#2C02
	const SDL_Color palette[64] = {
		{ 84,  84,  84}, {  0,  30, 116}, {  8,  16, 144}, { 48,   0, 136}, { 68,   0, 100}, { 92,   0,  48}, { 84,   4,   0}, { 60,  24,   0},
		{ 32,  42,   0}, {  8,  58,   0}, {  0,  64,   0}, {  0,  60,   0}, {  0,  50,  60}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
		{152, 150, 152}, {  8,  76, 196}, { 48,  50, 236}, { 92,  30, 228}, {136,  20, 176}, {160,  20, 100}, {152,  34,  32}, {120,  60,   0},
		{ 84,  90,   0}, { 40, 114,   0}, {  8, 124,   0}, {  0, 118,  40}, {  0, 102, 120}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
		{236, 238, 236}, { 76, 154, 236}, {120, 124, 236}, {176,  98, 236}, {228,  84, 236}, {236,  88, 180}, {236, 106, 100}, {212, 136,  32},
		{160, 170,   0}, {116, 196,   0}, { 76, 208,  32}, { 56, 204, 108}, { 56, 180, 204}, { 60,  60,  60}, {  0,   0,   0}, {  0,   0,   0},
		{236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 194, 144},
		{204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180}, {160, 214, 228}, {160, 162, 160}, {  0,   0,   0}, {  0,   0,   0}
	};

	// PPU IO open bus related. See https://wiki.nesdev.com/w/index.php?title=PPU_registers#Ports
	// and the 'NES PPU Open-Bus Test' test rom readme
	struct OpenBusIO
	{
		OpenBusIO() { decayed.fill(true); }

		const int decay_cycle_length = 29781 * 60 * 0.6; // roughly 600 ms = 36 frames; how long it takes for a bit to decay to 0.
		u8 value = 0; // the value read back when reading from open bus.
		std::array<bool, 8> decayed; // each bit can decay separately
		std::array<unsigned, 8> cycles_until_decay;

		u8 Read(u8 mask = 0xFF);
		void Write(u8 data);
		void UpdateValue(u8 data, u8 mask); /* Write to the bits of open bus given by the mask. Different from the write function, as there, all bits are refreshed. */
		void UpdateDecay();
		void UpdateDecayOnIOAccess(u8 mask);
	} open_bus_io;

	struct ScrollRegisters
	{
		/* Composition of 'v' (and 't'):
		  yyy NN YYYYY XXXXX
		  ||| || ||||| +++++-- coarse X scroll
		  ||| || +++++-------- coarse Y scroll
		  ||| ++-------------- nametable select
		  +++----------------- fine Y scroll
		*/
		unsigned v : 15; // Current VRAM address (15 bits): yyy NN YYYYY XXXXX
		unsigned t : 15; // Temporary VRAM address (15 bits); can also be thought of as the address of the top left onscreen tile.
		unsigned x : 3; // Fine X scroll (3 bits)
		bool w; // First or second write toggle (1 bit)

		void increment_coarse_x()
		{
			if ((v & 0x1F) == 0x1F) // if coarse X == 31
			{
				v &= ~0x1F; // set course x = 0
				v ^= 0x400; // switch horizontal nametable by toggling bit 10
			}
			else v++; // increment coarse X
		}

		void increment_y()
		{
			if ((v & 0x7000) == 0x7000) // if fine y == 7
			{
				v &= ~0x7000; // set fine y = 0
				switch ((v >> 5) & 0x1F) // branch on coarse y
				{
				case 29:
					v &= ~(0x1F << 5); // set course y = 0
					v ^= 0x800; // switch vertical nametable
					break;
				case 31:
					v &= ~(0x1F << 5); // set course y = 0
					break;
				default:
					v += 0x20; // increment coarse y
					break;
				}
			}
			else v += 0x1000;
		}
	} scroll;

	struct SpriteEvaluation
	{
		unsigned num_sprites_copied = 0; // 0-8, the number of sprites copied from OAM into secondary OAM
		unsigned n : 6; // index (0-63) of the sprite currently being checked in OAM
		unsigned m : 2; // byte (0-3) of this sprite
		bool idle = false; // whether the sprite evaluation is finished for the current scanline
		bool sprite_0_included = false; // whether the 0th byte was copied from OAM into secondary OAM

		void Reset() { num_sprites_copied = n = m = idle = sprite_0_included = 0; }
	} sprite_evaluation;

	struct TileFetcher
	{
		// fetched data of the tile currently being fetched
		u8 tile_num; // nametable byte; hex digits 2-1 of the address of the tile's pattern table entries. 
		u8 attribute_table_byte; // palette data for the tile. depending on which quadrant of a 16x16 pixel metatile this tile is in, two bits of this byte indicate the palette number (0-3) used for the tile
		u8 pattern_table_tile_low, pattern_table_tile_high; // actual colour data describing the tile. If bit n of tile_high is 'x' and bit n of tile_low is 'y', the colour id for pixel n of the tile is 'xy'

		// used only for background tiles
		u8 attribute_table_quadrant;

		// used only for sprites
		u8 sprite_y_pos;
		u8 sprite_attr;

		u16 pattern_table_data_addr;

		enum class Step {
			fetch_nametable_byte, fetch_attribute_table_byte, fetch_pattern_table_tile_low, fetch_pattern_table_tile_high
		} step;

		void SetBGTileFetchingActive() { step = Step::fetch_nametable_byte; }
		void SetSpriteTileFetchingActive() { step = Step::fetch_pattern_table_tile_low; }
	} tile_fetcher;

	bool cycle_340_was_skipped_on_last_scanline = false; // On NTSC, cycle 340 of the pre render scanline may be skipped every other frame.
	bool do_not_set_vblank_flag_on_next_vblank = false; // The VBL flag may not be set if PPUSTATUS is read to close to when the flag is supposed to be set.
	bool NMI_line = 1;
	bool odd_frame = false;
	bool reset_graphics_after_render = false;
	bool set_sprite_0_hit_flag = false;
	bool suppress_nmi_on_next_vblank = false; // An NMI may not trigger on VBL if PPUSTATUS is read to close to when the VBL flag is set.

	u8 pixel_x_pos = 0;
	u8 PPUCTRL;
	u8 PPUMASK;
	u8 PPUSTATUS;
	u8 PPUSCROLL;
	u8 PPUDATA;
	u8 OAMADDR;
	u8 OAMADDR_at_cycle_65;
	u8 OAMDMA;

	u8 sprite_attribute_latch[8]{};
	u8 sprite_pattern_shift_reg[2][8]{};

	u16 bg_palette_attr_reg[2]{}; // These are actually 8 bits on real HW, but it's easier this way. Similar to the pattern shift registers, the MSB contain data for the current tile, and the bottom LSB for the next tile.
	u16 bg_pattern_shift_reg[2]{};

	int scanline = 0;

	int sprite_x_pos_counter[8]{};

	unsigned framebuffer_pos = 0;
	unsigned scanline_cycle;
	unsigned window_scale;
	unsigned window_scale_temp;
	unsigned window_pixel_offset_x;
	unsigned window_pixel_offset_x_temp;
	unsigned window_pixel_offset_y;
	unsigned window_pixel_offset_y_temp;

	std::array<u8, 0x100 > oam{}; /* Not mapped. Holds sprite data (four bytes each for up to 64 sprites). */
	std::array<u8, 0x20  > palette_ram{}; /* Mapped to PPU $3F00-$3F1F (mirrored at $3F20-$3FFF). */
	std::array<u8, 0x20  > secondary_oam{}; /* Holds sprite data for sprites to be rendered on the next scanline. */
	std::array<u8, 0x1000> nametable_ram{}; /* Mapped to PPU $2000-$2FFF (mirrored at $3000-$3EFF). */

	std::vector<u8> framebuffer{};

	SDL_Renderer* renderer;
	SDL_Window* window;

	void CheckNMI();
	void LogState();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
	void PushPixel(u8 nes_col);
	void ReloadBackgroundShiftRegisters();
	void ReloadSpriteShiftRegisters(unsigned sprite_index);
	void RenderGraphics();
	void ResetGraphics();
	void ShiftPixel();
	void StepCycle();
	void UpdateBGTileFetching();
	void UpdateSpriteEvaluation();
	void UpdateSpriteTileFetching();
	void WriteMemory(u16 addr, u8 data);
	void WritePaletteRAM(u16 addr, u8 data);

	bool RenderingIsEnabled();

	[[nodiscard]] u8 GetNESColorFromColorID(u8 col_id, u8 palette_id, TileType tile_type);
	[[nodiscard]] u8 ReadMemory(u16 addr);
	[[nodiscard]] u8 ReadPaletteRAM(u16 addr);

	size_t GetFrameBufferSize() const { return num_pixels_per_scanline * standard.num_visible_scanlines * num_colour_channels; };
};