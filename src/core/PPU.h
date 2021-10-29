#pragma once

#include "wx/wx.h"
#include "SDL.h"

#include <format>
#include <limits>
#include <stdexcept>

#include "../Observer.h"
#include "../Types.h"

#include "../debug/Logging.h"

#include "Bus.h"
#include "Component.h"
#include "CPU.h"
#include "System.h"

#include "mappers/BaseMapper.h"

class PPU final : public Component
{
public:
	~PPU();

	std::shared_ptr<BaseMapper> mapper; /* This is heap-allocated, the other components are not. */
	CPU* cpu;
	Observer* gui;

	[[nodiscard]] bool CreateRenderer(const void* window_handle);
	void PowerOn(const System::VideoStandard standard);
	void Reset();
	void Update();

	// writing and reading done by the CPU to/from the registers at $2000-$2007, $4014
	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

	/* Note: vblank only begins after the "post-render" scanlines, i.e. on the same scanline as NMI is triggered. */
	bool IsInVblank() { return scanline >= standard.nmi_scanline; }

	unsigned GetWindowScale() { return scale; }
	wxSize GetWindowSize() { return wxSize(num_pixels_per_scanline * scale, standard.num_visible_scanlines * scale); }
	void SetWindowScale(unsigned scale) { this->scale = scale; }
	void SetWindowSize(wxSize size);

	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

private:
	friend class Logging;

	static const size_t oam_size = 0x100;
	static const size_t secondary_oam_size = 0x20;

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
	};

	const Standard NTSC  = {  true,  true, 3.0f, 241, 262, 20, 240 };
	const Standard PAL   = { false, false, 3.2f, 240, 312, 70, 239 };
	const Standard Dendy = {  true, false, 3.0f, 290, 312, 20, 239 };
	Standard standard = NTSC; /* The default */

	/* Some PPU operation details that are not affected by the video standard */
	const int pre_render_scanline = -1;
	const int num_colour_channels = 3;
	const int num_cycles_per_scanline = 341; // On NTSC: is actually 340 on scanline 261 if on an odd-numbered frame
	const int num_pixels_per_scanline = 256; // Horizontal resolution
	const int open_bus_decay_cycle_length = 30000; // roughly a frame

	/* Settings-related */
	const unsigned default_scale = 3;

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

	enum class TileType { BG, OBJ };

	struct Memory
	{
		u8 vram[0x1000]{}; // $2000-$2FFF; mapped to $2000-$3EFFF; nametables
		u8 palette_ram[0x20]{}; // $3F00-$3F1F; mapped to $3F00-$3FFF
		u8 oam[oam_size]{}; // not mapped
		u8 secondary_oam[secondary_oam_size]{}; // not mapped
	} memory;

	struct ScrollRegs
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
	} reg;

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

		void SetBGTileFetchingActive()     { step = Step::fetch_nametable_byte; }
		void SetSpriteTileFetchingActive() { step = Step::fetch_pattern_table_tile_low; }
	} tile_fetcher;

	bool cycle_340_was_skipped_on_last_scanline = false;
	bool NMI_line = 1;
	bool odd_frame = false; // during odd-numbered frames, the pre-render scanline lasts for 339 ppu cycles instead of 340 as normally
	bool open_bus_decayed = true;
	bool set_sprite_0_hit_flag = false;

	// PPU registers accessible by the CPU
	u8 PPUCTRL;
	u8 PPUMASK;
	u8 PPUSTATUS;
	u8 PPUSCROLL;
	u8 PPUDATA;
	u8 OAMADDR;
	u8 OAMDMA;

	u8 open_bus = 0; // See https://wiki.nesdev.com/w/index.php?title=PPU_registers#Ports
	u8 OAMADDR_at_cycle_65;
	u8 pixel_x_pos = 0;

	signed scanline = 0;
	unsigned scanline_cycle;
	unsigned cycles_until_open_bus_decay;

	u16 bg_pattern_shift_reg[2]{};
	// These are actually 8 bits on real HW, but it's easier this way.
	// Similar to the pattern shift registers, the MSB contain data for the current tile, and the bottom LSB for the next tile.
	u16 bg_palette_attr_reg[2]{};
	u8 sprite_pattern_shift_reg[2][8]{};
	u8 sprite_attribute_latch[8]{};
	signed sprite_x_pos_counter[8]{};

	// SDL renderering specific
	SDL_Renderer* renderer;
	SDL_Window* window;
	SDL_Rect rect;
	unsigned scale, scale_temp;
	unsigned pixel_offset_x, pixel_offset_y, pixel_offset_x_temp, pixel_offset_y_temp;
	bool reset_graphics_after_render;
	std::vector<u8> framebuffer{};
	unsigned frame_buffer_pos = 0;

	const size_t GetFrameBufferSize() const { return num_pixels_per_scanline * standard.num_visible_scanlines * num_colour_channels; };

	void CheckNMI();
	void UpdateSpriteEvaluation();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
	void PushPixel(u8 nes_col);
	void ReloadBackgroundShiftRegisters();
	void ReloadSpriteShiftRegisters(unsigned sprite_index);
	void RenderGraphics();
	bool RenderingIsEnabled();
	void ResetGraphics();
	void ShiftPixel();
	void StepCycle();
	void UpdateBGTileFetching();
	void UpdateSpriteTileFetching();
	void WriteMemory(u16 addr, u8 data);
	void WriteOpenBus(u8 data);
	void WritePaletteRAM(u16 addr, u8 data);

	[[nodiscard]] u8 GetNESColorFromColorID(u8 col_id, u8 palette_id, TileType tile_type);
	[[nodiscard]] u8 ReadMemory(u16 addr);
	[[nodiscard]] u8 ReadOpenBus();
	[[nodiscard]] u8 ReadPaletteRAM(u16 addr);

	/// Debugging-related
	void LogState();
};

