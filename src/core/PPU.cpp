#include "PPU.h"

// register bitmasks
#define PPUCTRL_NMI_enable_mask             0x80
#define PPUCTRL_PPU_master_mask             0x40
#define PPUCTRL_sprite_height_mask          0x20
#define PPUCTRL_bg_tile_sel_mask            0x10
#define PPUCTRL_sprite_tile_sel_mask        0x08
#define PPUCTRL_incr_mode_mask              0x04
#define PPUCTRL_nametable_sel_mask          0x03

#define PPUMASK_col_emphasis_mask           0x70
#define PPUMASK_sprite_enable_mask          0x10
#define PPUMASK_bg_enable_mask              0x08
#define PPUMASK_sprite_left_col_enable_mask 0x04
#define PPUMASK_bg_left_col_enable_mask     0x02
#define PPUMASK_greyscale_mask              0x01

#define PPUSTATUS_vblank_mask               0x80
#define PPUSTATUS_sprite_0_hit_mask         0x40
#define PPUSTATUS_sprite_overflow_mask      0x20


#define PPUCTRL_NMI_enable             (PPUCTRL & PPUCTRL_NMI_enable_mask)
#define PPUCTRL_PPU_master             (PPUCTRL & PPUCTRL_PPU_master_mask)
#define PPUCTRL_sprite_height          (PPUCTRL & PPUCTRL_sprite_height_mask)
#define PPUCTRL_bg_tile_sel            (PPUCTRL & PPUCTRL_bg_tile_sel_mask)
#define PPUCTRL_sprite_tile_sel        (PPUCTRL & PPUCTRL_sprite_tile_sel_mask)
#define PPUCTRL_incr_mode              (PPUCTRL & PPUCTRL_incr_mode_mask)
#define PPUCTRL_nametable_sel          (PPUCTRL & PPUCTRL_nametable_sel_mask)

#define PPUMASK_col_emphasis           (PPUMASK & PPUMASK_col_emphasis_mask)
#define PPUMASK_sprite_enable          (PPUMASK & PPUMASK_sprite_enable_mask)
#define PPUMASK_bg_enable              (PPUMASK & PPUMASK_bg_enable_mask)
#define PPUMASK_sprite_left_col_enable (PPUMASK & PPUMASK_sprite_left_col_enable_mask)
#define PPUMASK_bg_left_col_enable     (PPUMASK & PPUMASK_bg_left_col_enable_mask)
#define PPUMASK_greyscale              (PPUMASK & PPUMASK_greyscale_mask)

#define PPUSTATUS_vblank               (PPUSTATUS & PPUSTATUS_vblank_mask)
#define PPUSTATUS_sprite_0_hit         (PPUSTATUS & PPUSTATUS_sprite_0_hit_mask)
#define PPUSTATUS_sprite_overflow      (PPUSTATUS & PPUSTATUS_sprite_overflow_mask)


#define pre_render_scanline -1
#define post_render_scanline 240


void PPU::Initialize()
{
	
}


void PPU::Update()
{
	// PPU::Update() is called once each cpu cycle, but 1 cpu cycle = 3 ppu cycles
	for (int i = 0; i < 3; i++)
	{
		if (ppu_cycle_counter == 0) continue; // idle cycle on every scanline (?)

		if (current_scanline == pre_render_scanline) // == -1
		{
			if (ppu_cycle_counter == 1)
			{
				PPUSTATUS &= ~(PPUSTATUS_vblank_mask | PPUSTATUS_sprite_0_hit_mask | PPUSTATUS_sprite_overflow_mask);
			}
			else if (ppu_cycle_counter >= 280 && ppu_cycle_counter <= 304)
			{
				if (PPUCTRL_PPU_master)
				{
					// Set bits 14-11 of 'v' to bits 14-11 of 't', and bits 9-5 of 'v' to bits 9-5 of 't'
					// todo: Not clear if this should be done at every cycle in this interval or not
					reg.v = reg.v & ~(0xF << 11) & ~(0x1F << 5) | reg.t & (0xF << 11) | reg.t & (0x1F << 5);
				}
			}
		}
		else if (current_scanline < post_render_scanline) // < 240
		{
			if (ppu_cycle_counter <= 256)
			{
				if (ppu_cycle_counter == 1)
				{
					// Clear secondary OAM. Is supposed to happen one write at a time between cycles 1-64 (each write taking two cycles).
					// However, can all be done here, as secondary OAM can is not accessed from elsewhere during this time (?)
					for (int i = 0; i < 32; i++)
						secondary_oam[i] = 0xFF;

					tile_fetcher.tile_type = TileFetcher::TileType::BG;
					tile_fetcher.step = TileFetcher::Step::fetch_nametable_byte;
					tile_fetcher.x_pos = 2; // The first two tiles have already been fetched (during cycles 321-336 of the last scanline)
				}

				// On odd cycles, update the bg tile fetching (each step takes 2 cycles, starting at cycle 1).
				// On even cycles, update the sprite evaluation. On real HW, the process reads from OAM on odd cycles and writes to secondary OAM on even cycles. Here, both operations are done on even cycles.
				if (ppu_cycle_counter & 1)
					UpdateTileFetcher();
				else if (PPUMASK_sprite_enable || PPUMASK_bg_enable) // Sprite evaluation occurs if either the sprite layer or background layer is enabled
					DoSpriteEvaluation();

				if (ppu_cycle_counter > 1)
				{
					// Increment the coarse X scroll at cycles 8, 16, ..., 256
					// todo: they say "if rendering is enabled"
					if (ppu_cycle_counter % 8 == 0)
						reg.increment_coarse_x();
					// Update the bg shift registers at cycles 9, 17, ..., 249
					else if (ppu_cycle_counter % 8 == 1)
						UpdateBGShiftRegs();
				}
			}
			else if (ppu_cycle_counter <= 320)
			{
				static unsigned sprite_index = 0;

				if (ppu_cycle_counter == 257)
				{
					tile_fetcher.tile_type = TileFetcher::TileType::OAM; // Start fetching sprite tiles
					UpdateBGShiftRegs(); // Update the bg shift registers at cycle 257
					sprite_index = 0;
				}

				// Consider an 8 cycle period (1-8) between cycles 257-320 (of which there are eight; one for each sprite)
				// On odd cycles, update the tile fetching (each step takes 2 cycles, starting at cycle 257).
				// On cycle 1-4: read the Y-coordinate, tile number, attributes, and X-coordinate of the selected sprite from secondary OAM
				// On cycle 9 (i.e. the cycle after each period: 266, 274, ..., 321), update the sprite shift registers with pattern data
				// Not sure if all of this precise timing is necessary. However, the switch stmnt should make it fairly performant anyways
				switch ((ppu_cycle_counter - 257) % 8)
				{
				case 0: UpdateTileFetcher(); break;
				case 1: 
					tile_fetcher.tile_num = secondary_oam[4 * sprite_index + 1]; 
					if (ppu_cycle_counter >= 266) UpdateSpriteShiftRegs(sprite_index - 1); // the pattern data for the previous sprite is loaded
					break;
				case 2: 
					UpdateTileFetcher(); 
					sprite_attribute_latch[sprite_index] = secondary_oam[4 * sprite_index + 2];
					break;
				case 3: 
					sprite_x_pos_counter[sprite_index] = secondary_oam[4 * sprite_index + 3]; 
					break;
				case 4: UpdateTileFetcher(); break;
				case 5: break;
				case 6: UpdateTileFetcher(); break;
				case 7: sprite_index++; break;
				}
			}
			else // <= 340 (or 341, for even-numbered frames). 
			{
				if (ppu_cycle_counter == 321)
				{
					tile_fetcher.tile_type = TileFetcher::TileType::BG;
					UpdateSpriteShiftRegs(7); // the last sprite pattern data to be loaded (sprite index == 7)
				}

			    // todo: UpdateTileFetcher will be called on cycle 341, is it correct ?
				if (ppu_cycle_counter & 1)
					UpdateTileFetcher();

				// Increment the coarse X scroll at cycles 328 and 336, and update the bg shift registers at cycles 329 and 337
				switch (ppu_cycle_counter)
				{
				case 328: case 336: reg.increment_coarse_x(); break; // todo: they say "if rendering is enabled"
				case 329: case 337: UpdateBGShiftRegs(); break;
				default: break;
				}
			}

			if (current_scanline > pre_render_scanline && current_scanline < post_render_scanline)
			{
				// todo: Actual pixel output is delayed further due to internal render pipelining, and the first pixel is output during cycle 4.
				ShiftPixel();
			}
		}
		else if (current_scanline == post_render_scanline + 1) // == 241
		{
			if (ppu_cycle_counter == 1)
			{
				PPUSTATUS |= PPUSTATUS_vblank_mask;
				// todo: set VBlank NMI
			}
		}

		ppu_cycle_counter = (ppu_cycle_counter + 1) % (current_scanline == pre_render_scanline && odd_frame ? 340 : 341);
		if (ppu_cycle_counter == 0)
			PrepareForNewScanline();

		if (ppu_cycle_counter == 256)
		{
			// todo: the condition is 'rendering enabled', not sure what it means
			// todo: should we increment y during ALL scanlines?
			if (PPUCTRL_PPU_master)
			{
				reg.increment_y();
			}
		}
		else if (ppu_cycle_counter == 257)
		{
			// Set bit 10 of 'v' to bit 10 of 't', and bits 4-0 of 'v' to bits 4-0 of 't'
			reg.v = reg.v & ~(1 << 10) & ~0x1F | reg.t & (1 << 10) | reg.t & 0x1F;
		}
	}
}


u8 PPU::ReadFromPPUReg(u16 addr)
{
	switch (addr)
	{
	case Bus::Addr::PPUCTRL  : // $2000
	case Bus::Addr::PPUMASK  : // $2001
	case Bus::Addr::OAMADDR  : // $2003
	case Bus::Addr::PPUSCROLL: // $2005
	case Bus::Addr::PPUADDR  : // $2006
	case Bus::Addr::OAMDMA   : // $4014
		return 0xFF; // write-only. TODO: return 0xFF or 0?

	case Bus::Addr::PPUSTATUS: // $2002
		PPUSTATUS &= ~PPUSTATUS_vblank_mask;
		reg.w = 0;
		return PPUSTATUS;

	case Bus::Addr::OAMDATA: // $2004
		// during cycles 1-64, secondary OAM is initialised to 0xFF, and an internal signal makes reading from OAMDATA always return 0xFF during this time
		if (ppu_cycle_counter >= 1 && ppu_cycle_counter <= 64)
			return 0xFF;
		return OAMDATA;

	case Bus::Addr::PPUDATA  : // $2007
		if (current_scanline >= post_render_scanline || (!PPUMASK_bg_enable && !PPUMASK_sprite_enable)) // Outside of rendering
		{
			u16 v = reg.v;
			reg.v += PPUCTRL_incr_mode ? 32 : 1;
			return ReadMemory(v);
		}
		else // Rendering; current_scanline < post_render_scanline && (PPUMASK_bg_enable || PPUMASK_sprite_enable)
		{
			// Update v in an odd way, where a coarse X increment and a Y increment is triggered simultaneously
			reg.increment_coarse_x();
			reg.increment_y();
			return 0xFF;
		}

	default: //throw std::invalid_argument(std::format("Invalid argument addr %04X given to PPU::ReadFromPPUReg", (int)addr));
		;
	}
}


void PPU::WriteToPPUReg(u16 addr, u8 data)
{
	switch (addr)
	{
	case Bus::Addr::PPUCTRL: // $2000
		if (!PPUCTRL_NMI_enable && (data & PPUCTRL_NMI_enable_mask) && PPUSTATUS_vblank && this->IsInVblank())
		{
			// todo: generate NMI signal
		}
		PPUCTRL = data;
		// todo: After power/reset, writes to this register are ignored for about 30,000 cycles.
		reg.t = reg.t & ~(3 << 10) | (data & 3) << 10; // Set bits 11-10 of 't' to bits 1-0 of 'data'
		return;

	case Bus::Addr::PPUMASK: // $2001
		PPUMASK = data;
		return;

	case Bus::Addr::PPUSTATUS: // $2002
		PPUSTATUS = data;
		return;

	case Bus::Addr::OAMADDR: // $2003
		OAMADDR = data;
		return;

	case Bus::Addr::OAMDATA: // $2004
		if (current_scanline < post_render_scanline && (PPUMASK_bg_enable || PPUMASK_sprite_enable))
		{
			// Do not modify values in OAM, but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits
			OAMADDR += 0b100; // todo: correct?
		}
		else
		{
			memory.oam[OAMADDR++] = data;
		}
		return;

	case Bus::Addr::PPUSCROLL: // $2005
		if (reg.w == 0) // Update x-scroll registers
		{
			reg.t = reg.t & ~0x1F | data >> 3; // Set bits 4-0 of 't' (coarse x-scroll) to bits 7-3 of 'data'
			reg.x = data & 7; // Set 'x' (fine x-scroll) to bits 2-0 of 'data'
		}
		else // Update y-scroll registers
		{
			// Set bits 14-12 of 't' (fine y-scroll) to bits 2-0 of 'data', and bits 9-5 of 't' (coarse y-scroll) to bits 7-3 of 'data'
			reg.t = reg.t & ~(0x1F << 5) & ~(7 << 12) | (data & 7) << 12 | (data & 0xF8) << 5;
		}
		reg.w = !reg.w;
		return;

	case Bus::Addr::PPUADDR: // $2006
		if (reg.w == 0)
		{
			reg.t = reg.t & ~0x3F00 | (data & 0x3F) << 8; // Set bits 13-8 of 't' to bits 5-0 of 'data'
			reg.t &= ~(1 << 14); // Clear bit 14 of 't'
		}
		else
		{
			reg.t = reg.t & 0xFF00 | data; // Set the lower byte of 't' to 'data'
			reg.v = reg.t;
		}
		reg.w = !reg.w;
		return;

	case Bus::Addr::PPUDATA: // $2007
		if (this->IsInVblank() || (!PPUMASK_bg_enable && !PPUMASK_sprite_enable))
		{
			WriteMemory(reg.v, data);
			reg.v += PPUCTRL_incr_mode ? 32 : 1;
		}
		return;

	case Bus::Addr::OAMDMA: // $4014
	{
		// perform OAM DMA transfer. it is technically performed by the cpu, so the cpu will stop executing instructions during this time
		const u16 start_addr = OAMADDR << 8;
		for (u16 addr = start_addr; addr <= start_addr + 0xFF; addr++)
			memory.oam[addr & 0xFF] = bus->Read(addr);
		cpu->Set_OAM_DMA_Active();
	}

	}
}


void PPU::DoSpriteEvaluation()
{
	static unsigned num_sprites_copied = 0;
	static u8 n = 0, m = 0; // n: index (0-63) of the sprite currently being checked in OAM. m: byte (0-3) of this sprite
	static bool idle = false;

	auto increment_n = [&]()
	{
		if (++n == 64)
		{
			idle = true;
		}
	};

	auto increment_m = [&]()
	{
		if (++m == 4)
		{
			m = 0;
			if (n == 0)
				sprite_0_included = true;
			increment_n();
			num_sprites_copied++;
		}
	};

	if (idle) return;

	u8 read_oam = memory.oam[4 * n + m];

	if (num_sprites_copied < 8)
	{
		secondary_oam[num_sprites_copied + m] = read_oam;

		if (m == 0)
		{
			// check if the y-pos of the current sprite ('read_oam' = oam[4 * n + 0]) is in range
			if (read_oam >= current_scanline && read_oam < current_scanline + (PPUCTRL_sprite_height ? 16 : 8))
			{
				m = 1;
			}
			else
			{
				m = 0;
				increment_n();
			}
		}
		else
		{
			increment_m();
		}
	}
	else
	{
		if (read_oam >= current_scanline && read_oam < current_scanline + (PPUCTRL_sprite_height ? 16 : 8))
		{
			PPUSTATUS |= PPUSTATUS_sprite_overflow_mask;
			// On real hw, the ppu will continue scanning oam after setting the sprite overflow flag.
			// However, none of it will have an effect on anything other than n and m, so we may as well start idling from here.
			// Note also that the sprite overflow flag is not writeable by the cpu, and cleared only on the pre-render scanline
			idle = true;
		}
		else
		{
			// hw bug: increment both n and m (instead of just n)
			increment_m();
			increment_n();
		}
	}
}


void PPU::FetchSpriteDataFromSecondaryOAM()
{
	static unsigned sprite_index = 0;
	static unsigned cycle_counter = 0;

	switch (cycle_counter)
	{
	case 0: sprites.y_pos       [sprite_index] = secondary_oam[4 * sprite_index    ]; break;
	case 1: sprites.pattern_data[sprite_index] = secondary_oam[4 * sprite_index + 1]; break;
	case 2: sprites.attr        [sprite_index] = secondary_oam[4 * sprite_index + 2]; break;
	case 3: sprites.x_pos       [sprite_index] = secondary_oam[4 * sprite_index + 3]; break;
	}

	if (++cycle_counter == 8)
	{
		cycle_counter = 0;
		sprite_index = (sprite_index + 1) % 8;
	}

	// todo: fill data for empty slots
}


// Get an actual NES color (indexed 0-63) from a bg or sprite color id, given palette attribute data
u8 PPU::GetNESColorFromColorID(u8 col_id, u8 palette_attr_data, TileType tile_type)
{
	// if the color ID is 0, then the 'universal background color', located at $3F00, is always used.
	if (col_id == 0)
		return ReadMemory(0x3F00) & 0x3F;
	else
	{
		// For bg tiles, two consecutive bits of an attribute table byte holds the palette number (0-3). These have already been extracted beforehand (see the updating of the 'bg_palette_attr_reg' variable)
		// For sprites, bits 1-0 of the 'attribute byte' (byte 2 from OAM) give the palette number.
		// Each bg and sprite palette consists of three bytes (describing the actual NES colors for color ID:s 1, 2, 3), starting at $3F01, $3F05, $3F09, $3F0D respectively for bg tiles, and $3F11, $3F15, $3F19, $3F1D for sprites
		u8 palette_id = palette_attr_data & 3;
		return ReadMemory(0x3F01 + 0x10 * (tile_type == TileType::BG) + 4 * palette_id) & 0x3F;
	}
}


void PPU::PushPixel(u8 colour)
{
	const SDL_Color& col = palette[colour];
	framebuffer[frame_buffer_pos    ] = col.r;
	framebuffer[frame_buffer_pos + 1] = col.g;
	framebuffer[frame_buffer_pos + 2] = col.b;
	frame_buffer_pos += 3;

	pixel_x_pos++;
}


void PPU::RenderGraphics()
{
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(framebuffer, resolution_x, resolution_y,
		8 * colour_channels, resolution_x * colour_channels, 0x0000FF, 0x00FF00, 0xFF0000, 0);

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	rect.w = resolution_x * scale;
	rect.h = resolution_y * scale;
	rect.x = pixel_offset_x;
	rect.y = pixel_offset_y;
	SDL_RenderCopy(renderer, texture, NULL, &rect);

	SDL_RenderPresent(renderer);

	//if (reset_graphics_after_render)
	//	ResetGraphics();

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}


void PPU::ShiftPixel()
{
	// Fetch one bit from each of the two 16-bit bg shift registers (containing pattern table data for the current tile), forming the colour id for the current bg pixel
	u8 bg_col_id = (bg_pattern_shift_reg[1] >> reg.x & 1) << 1 | bg_pattern_shift_reg[0] >> reg.x & 1;
	bg_pattern_shift_reg[0] >>= 1;
	bg_pattern_shift_reg[1] >>= 1;

	// Decrement the x-position counters for all 8 sprites. If a counter is 0, the sprite becomes 'active', and the shift registers for the sprite is shifted once every cycle
	// The current pixel for each 'active' sprite is checked, and the first non-transparent pixel moves on to a multiplexer, where it joins the BG pixel.
	u8 sprite_col_id = 0;
	u8 sprite_index = 0; // (0-7)
	bool sprite_found = false;
	for (int i = 0; i < 8; i++)
	{
		if (sprite_x_pos_counter[i] > 0)
		{
			sprite_x_pos_counter[i]--;
		}
		else
		{
			if (!sprite_found)
			{
				u8 curr_sprite_col_id = (sprite_pattern_shift_reg[1][i] & 1) << 1 | sprite_pattern_shift_reg[0][i] & 1;
				if (curr_sprite_col_id != 0)
				{
					sprite_col_id = curr_sprite_col_id;
					sprite_index = i;
					sprite_found = true;
				}
			}
			sprite_pattern_shift_reg[0][i] >>= 1;
			sprite_pattern_shift_reg[1][i] >>= 1;
		}
	}

	// Set the sprite zero hit flag if all conditions below are met
	if (!PPUSTATUS_sprite_0_hit                                                             && // The flag has not already been set this frame
		sprite_0_included && sprite_index == 0                                              && // The current sprite is the 0th sprite in OAM
		bg_col_id != 0 && sprite_col_id != 0                                                && // The bg and sprite colour IDs are not 0, i.e. both pixels are opaque
		PPUMASK_bg_enable && PPUMASK_sprite_enable                                          && // Both bg and sprite rendering must be enabled
		(pixel_x_pos > 8 || (PPUMASK_bg_left_col_enable && PPUMASK_sprite_left_col_enable)) && // If the pixel-x-pos is between 0 and 7, the left-side clipping window must be disabled for both bg tiles and sprites.
		pixel_x_pos != 255)                                                                    // The pixel-x-pos must not be 255
	{
		PPUSTATUS |= PPUSTATUS_sprite_0_hit_mask;
	}

	// Mix the bg and sprite pixels, and get an actual NES color from the color id and palette attribute data
	// Decision table for mixing:
	/* BG pixel | Sprite pixel | Priority | Output
	  ---------------------------------------------
	      0     |       0      |    Any   |   BG
		  0     |      1-3     |    Any   | Sprite
		 1-3    |       0      |    Any   |   BG
		 1-3    |      1-3     |     0    | Sprite
		 1-3    |      1-3     |     1    |   BG
	*/
	u8 col;
	bool sprite_priority = sprite_attribute_latch[sprite_index] & 0x20;
	if (sprite_col_id > 0 && (bg_col_id == 0 || !sprite_priority))
		col = GetNESColorFromColorID(sprite_col_id, sprite_attribute_latch[sprite_index], TileType::OBJ);
	else
		col = GetNESColorFromColorID(bg_col_id, bg_palette_attr_reg, TileType::BG);

	PushPixel(col);
}


void PPU::UpdateBGShiftRegs()
{
	// Reload the upper 8 bits of the two 16-bit background shifters with pattern data for the next tile
	bg_pattern_shift_reg[0] = bg_pattern_shift_reg[0] & 0xFF | tile_fetcher.pattern_table_tile_low << 8;
	bg_pattern_shift_reg[1] = bg_pattern_shift_reg[1] & 0xFF | tile_fetcher.pattern_table_tile_high << 8;

	// For bg tiles, an attribute table byte holds palette info. Each table entry controls a 32x32 pixel metatile.
	// The byte is divided into four 2-bit areas, which each control a 16x16 pixel metatile
	// Denoting the four 16x16 pixel metatiles by 'bottomright', 'bottomleft' etc, then: value = (bottomright << 6) | (bottomleft << 4) | (topright << 2) | (topleft << 0)
	// We find which quadrant our 8x8 tile lies in. Then, the two extracted bits give the palette number (0-3) used for the tile
	bg_palette_attr_reg = tile_fetcher.attribute_table_byte >> (2 * tile_fetcher.attribute_table_quadrant) & 3;
}


void PPU::UpdateSpriteShiftRegs(unsigned sprite_index)
{
	// Reload the two 8-bit sprite shifters with pattern data for the next tile
	sprite_pattern_shift_reg[0][sprite_index] = tile_fetcher.pattern_table_tile_low;
	sprite_pattern_shift_reg[1][sprite_index] = tile_fetcher.pattern_table_tile_high;
}


void PPU::UpdateTileFetcher()
{
	auto GetBasePatternTableIndex = [&]()
	{
		switch (tile_fetcher.tile_type)
		{
		case TileFetcher::TileType::BG: return PPUCTRL_bg_tile_sel ? 0x1000 : 0x0000;
		case TileFetcher::TileType::OAM: return PPUCTRL_sprite_tile_sel ? 0x1000 : 0x0000;
		}
	};

	switch (tile_fetcher.step)
	{
	case TileFetcher::Step::fetch_nametable_byte:
	{
		/* Composition of the nametable address:
		  10 NN YYYYY XXXXX
		  || || ||||| +++++-- Coarse X scroll
		  || || +++++-------- Coarse Y scroll
		  || ++-------------- Nametable select
		  ++----------------- Nametable base address ($2000)
		*/
		u16 addr = 0x2000 | (reg.v & 0x0FFF);
		tile_fetcher.tile_num = ReadMemory(addr);
		break;
	}

	case TileFetcher::Step::fetch_attribute_table_byte:
	{
		/* Composition of the attribute address:
		  10 NN 1111 YYY XXX
		  || || |||| ||| +++-- High 3 bits of coarse X scroll (x/4)
		  || || |||| +++------ High 3 bits of coarse Y scroll (y/4)
		  || || ++++---------- Attribute offset (960 = $3c0 bytes)
		  || ++--------------- Nametable select
		  ++------------------ Nametable base address ($2000)
		*/
		u16 addr = 0x23C0 | (reg.v & 0x0C00) | ((reg.v >> 4) & 0x38) | ((reg.v >> 2) & 7);
		tile_fetcher.attribute_table_byte = ReadMemory(addr);

		// Determine in which quadrant (0-3) of the 32x32 pixel metatile that the current tile is in
		// topleft == 0, topright == 1, bottomleft == 2, bottomright = 3
		// scroll-x % 4 and scroll-y % 4 give the "tile-coordinates" of the current tile in the metatile
		tile_fetcher.attribute_table_quadrant = 2 * ((reg.v & 0x60) > 0x20) + ((reg.v & 3) > 1);
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_low:
	{
		/* Composition of the pattern table address for BG tiles and 8x8 sprites:
		H RRRR CCCC P yyy
		| |||| |||| | +++-- Fine Y scroll, the row number within a tile
		| |||| |||| +------ Bit plane (0: "lower"; 1: "upper")
		| |||| ++++-------- Tile column
		| ++++------------- Tile row
		+------------------ Half of sprite table (0: "left"; 1: "right"); dependent on PPUCTRL flags
		For BG tiles    : RRRR CCCC == the nametable byte fetched in step 1
		For 8x8 sprites : RRRR CCCC == the sprite tile index number fetched from secondary OAM during cycles 257-320

		Composition of the pattern table adress for 8x16 sprites:
		H RRRR CCC S P yyy
		| |||| ||| | | +++-- Fine Y scroll, the row number within a tile
		| |||| ||| | +------ Bit plane (0: "lower"; 1: "upper")
		| |||| ||| +-------- Sprite tile half (0: "top"; 1: "bottom") TODO: check if it isn't the reverse
		| |||| +++---------- Tile column
		| ++++-------------- Tile row
		+------------------- Half of sprite table (0: "left"; 1: "right"); equal to bit 0 of the sprite tile index number fetched from secondary OAM during cycles 257-320
		RRRR CCCC == upper 7 bits of the sprite tile index number fetched from secondary OAM during cycles 257-320
		*/
		u16 addr;
		if (tile_fetcher.tile_type == TileFetcher::TileType::OAM && PPUCTRL_sprite_height) // 8x16 sprites
		{
			addr = (tile_fetcher.tile_num & 1) << 12 | (tile_fetcher.tile_num & ~1) << 4 | reg.v >> 12;
			// Check if we are on the top or bottom tile of the 8x16 sprite
			if (reg.v & 0x20) // i.e. the course Y scroll is odd, meaning that we are on the bottom tile
				addr |= 0x10;
		}
		else
			addr = GetBasePatternTableIndex() | tile_fetcher.tile_num << 4 | reg.v >> 12;
		tile_fetcher.pattern_table_tile_low = ReadMemory(addr);
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_high:
	{
		u16 addr;
		if (tile_fetcher.tile_type == TileFetcher::TileType::OAM && PPUCTRL_sprite_height) // 8x16 sprites
		{
			addr = (tile_fetcher.tile_num & 1) << 12 | (tile_fetcher.tile_num & ~1) << 4 | reg.v >> 12 | 8;
			if (reg.v & 0x20)
				addr |= 0x10;
		}
		else
			addr = GetBasePatternTableIndex() | tile_fetcher.tile_num << 4 | reg.v >> 12 | 8;
		tile_fetcher.pattern_table_tile_high = ReadMemory(addr);
		break;
	}
	}

	tile_fetcher.step = static_cast<TileFetcher::Step>((tile_fetcher.step + 1) % 4);
}

u8 PPU::ReadMemory(u16 addr)
{
	// reading and writing done internally by the ppu, e.g. when PPUDATA is written/read to/from
	if (addr <= 0x2FFF)
	{
		return u8();
	}
	else if (addr <= 0x3EFF)
	{
		return u8();
	}
	else if (addr <= 0x3F1F)
	{
		// Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
		// Note: bits 4-0 of all mirrors have the form 1xy00, and the redirected addresses have the form 0xy00
		if ((addr & 0b10011) == 0b10000)
			return memory.palette_ram[addr - 0x3F00 - 0x10];
		return memory.palette_ram[addr - 0x3F00];
	}
	else if (addr <= 0x3FFF)
	{

	}
	else
	{
		// throw exception
		return 0xFF;
	}
}


void PPU::WriteMemory(u16 addr, u8 data)
{

}


void PPU::PrepareForNewFrame()
{
	odd_frame = !odd_frame;
}


void PPU::PrepareForNewScanline()
{
	current_scanline = (current_scanline + 1) % 261;
	if (current_scanline == 0)
		PrepareForNewFrame();
}