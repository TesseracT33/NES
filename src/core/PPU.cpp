#include "PPU.h"

// register bitmasks
#define PPUCTRL_NMI_enable_mask             0x80
#define PPUCTRL_PPU_master_mask             0x40
#define PPUCTRL_sprite_height_mask          0x20
#define PPUCTRL_bg_tile_sel_mask            0x10
#define PPUCTRL_sprite_tile_sel_mask        0x08
#define PPUCTRL_incr_mode_mask              0x04
#define PPUCTRL_nametable_sel_mask          0x03

#define PPUMASK_emphasize_blue_mask         0x80
#define PPUMASK_emphasize_green_mask        0x40
#define PPUMASK_emphasize_red_mask          0x20
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

#define PPUMASK_emphasize_blue         (PPUMASK & PPUMASK_emphasize_blue_mask)
#define PPUMASK_emphasize_green        (PPUMASK & PPUMASK_emphasize_green_mask)
#define PPUMASK_emphasize_red          (PPUMASK & PPUMASK_emphasize_red_mask)
#define PPUMASK_sprite_enable          (PPUMASK & PPUMASK_sprite_enable_mask)
#define PPUMASK_bg_enable              (PPUMASK & PPUMASK_bg_enable_mask)
#define PPUMASK_sprite_left_col_enable (PPUMASK & PPUMASK_sprite_left_col_enable_mask)
#define PPUMASK_bg_left_col_enable     (PPUMASK & PPUMASK_bg_left_col_enable_mask)
#define PPUMASK_greyscale              (PPUMASK & PPUMASK_greyscale_mask)

#define PPUSTATUS_vblank               (PPUSTATUS & PPUSTATUS_vblank_mask)
#define PPUSTATUS_sprite_0_hit         (PPUSTATUS & PPUSTATUS_sprite_0_hit_mask)
#define PPUSTATUS_sprite_overflow      (PPUSTATUS & PPUSTATUS_sprite_overflow_mask)


PPU::~PPU()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}


void PPU::Power()
{
	Reset();

	OAMADDR = reg.v = reg.t = 0;
	//PPUSTATUS = 0b10100000;
	PPUSTATUS = 0;
}


void PPU::Reset()
{
	PPUCTRL = PPUMASK = PPUSCROLL = PPUDATA = reg.w = 0;
	odd_frame = false;
	scanline_cycle_counter = 27; // Todo: why 27? No idea, that's where Mesen starts (as is evident in its debugger)
	current_scanline = 0;
}


bool PPU::CreateRenderer(const void* window_handle)
{
	this->window = SDL_CreateWindowFrom(window_handle);
	if (window == NULL)
	{
		wxMessageBox("Could not create the SDL window!");
		return false;
	}

	this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == NULL)
	{
		wxMessageBox("Renderer could not be created!");
		SDL_DestroyWindow(window);
		window = nullptr;
		return false;
	}

	return true;
}


void PPU::Update()
{
#ifdef DEBUG
	LogState();
#endif

	// PPU::Update() is called once each cpu cycle, but 1 cpu cycle = 3 ppu cycles
	for (int i = 0; i < 3; i++)
	{
		if (scanline_cycle_counter == 0)
		{
			// idle cycle on every scanline, except for on scanline 0 on odd-numbered frames when rendering is enabled, where it is skipped.
			scanline_cycle_counter = 1;
			if (!(current_scanline == 0 && odd_frame && RenderingIsEnabled()))
				continue;
		}

		if (current_scanline < post_render_scanline || current_scanline == pre_render_scanline) // Scanlines 0-239, 261
		{
			if (scanline_cycle_counter <= 256) // Cycles 1-256
			{
				// On even cycles, update the bg tile fetching (each step actually takes 2 cycles, starting at cycle 1).
				// On even cycles >= 66, update the sprite evaluation. On real HW, the process reads from OAM on odd cycles and writes to secondary OAM on even cycles (starting from cycle 65).
				// On all cycles, push a pixel.
				switch (scanline_cycle_counter)
				{
				case 1:
					// Clear secondary OAM. Is supposed to happen one write at a time between cycles 1-64.
					// However, can all be done here, as secondary OAM can is not accessed from elsewhere during this time
					for (int i = 0; i < secondary_oam_size; i++)
						memory.secondary_oam[i] = 0xFF;

					tile_fetcher.SetBGTileFetchingActive();

					if (current_scanline == pre_render_scanline)
					{
						PPUSTATUS &= ~(PPUSTATUS_vblank_mask | PPUSTATUS_sprite_0_hit_mask | PPUSTATUS_sprite_overflow_mask);
						CheckNMIInterrupt();
						RenderGraphics();
					}
					break;

				case 65:
					OAMADDR_at_cycle_65 = OAMADDR; // used in sprite evaluation as the offset addr in OAM
					sprite_evaluation.Reset();
					ReloadBackgroundShiftRegisters();
					break;

				case 256:
					UpdateBGTileFetching();
					if (RenderingIsEnabled())
						UpdateSpriteEvaluation();
					reg.increment_coarse_x();
					reg.increment_y();
					break;

				default:
					if (!(scanline_cycle_counter & 1))
					{
						UpdateBGTileFetching();
						if (scanline_cycle_counter >= 66 && RenderingIsEnabled())
							UpdateSpriteEvaluation();
					}

					// Increment the coarse X scroll at cycles 8, 16, ..., 256
					if (RenderingIsEnabled() && scanline_cycle_counter % 8 == 0)
						reg.increment_coarse_x();
					// Update the bg shift registers at cycles 9, 17, ..., 249, 257 (the one for cycle 257 is done later)
					else if (scanline_cycle_counter % 8 == 1)
						ReloadBackgroundShiftRegisters();

					break;
				}

				// todo: Actual pixel output is delayed further due to internal render pipelining, and the first pixel is output during cycle 4.
				// Shift one pixel per cycle during cycles 1-256 on scanlines 0-239
				ShiftPixel();
			}
			else if (scanline_cycle_counter <= 320) // Cycles 257-320
			{
				OAMADDR = 0; // is set to 0 at every cycle in this interval on visible scanlines + on the pre-render one

				static unsigned sprite_index;

				if (scanline_cycle_counter == 257)
				{
					tile_fetcher.SetSpriteTileFetchingActive();
					ReloadBackgroundShiftRegisters(); // Update the bg shift registers at cycle 257
					if (RenderingIsEnabled())
						reg.v = reg.v & ~0x41F | reg.t & 0x41F; // copy all bits related to horizontal position from t to v:
					sprite_index = 0;
				}

				// Consider an 8 cycle period (0-7) between cycles 257-320 (of which there are eight: one for each sprite)
				// On cycle 0-3: read the Y-coordinate, tile number, attributes, and X-coordinate of the selected sprite from secondary OAM.
				//    Note: All of this can be done on cycle 0, as none of this data is used until cycle 5 at the earliest (some of it is not used until the next scanline).
				// On cycles 5 and 7, update the sprite tile fetching (each step takes 2 cycles).
				//    Note: it is also supposed to update at cycles 1 and 3, but it then fetches garbage data. Todo: is there any point in emulating this? Reading is done from interval VRAM, not CHR
				// On cycle 8 (i.e. the cycle after each period: 266, 274, ..., 321), update the sprite shift registers with pattern data.
				//    Note: all of this can be done on cycle 321, for none of this data is used until on the next scanline
				switch ((scanline_cycle_counter - 257) % 8)
				{
				case 0:
					tile_fetcher.y_pos                   = memory.secondary_oam[4 * sprite_index    ];
					tile_fetcher.tile_num                = memory.secondary_oam[4 * sprite_index + 1];
					sprite_attribute_latch[sprite_index] = memory.secondary_oam[4 * sprite_index + 2];
					sprite_x_pos_counter  [sprite_index] = memory.secondary_oam[4 * sprite_index + 3];
					sprite_index++;
					break;

				case 5: case 7:
					UpdateSpriteTileFetching(); 
					break;

				default: break;
				}

				if (current_scanline == pre_render_scanline && scanline_cycle_counter >= 280 && scanline_cycle_counter <= 304 && RenderingIsEnabled())
				{
					// Copy the vertical bits of t to v
					reg.v = reg.v & ~0x7BE0 | reg.t & 0x7BE0;
				}
			}
			else // Cycles 321-340
			{
				// On even cycles, do bg tile fetching. Two tiles are fetched in total. The shift registers are reloaded at cycles 329 and 337.
				// Increment the coarse X scroll at cycles 328 and 336.
				// Todo: the very last byte fetched (at cycle 340) should be the same as the previous one (at cycle 338)
				switch (scanline_cycle_counter)
				{
				case 321:
					ReloadSpriteShiftRegisters();
					tile_fetcher.SetBGTileFetchingActive();
					break;

				case 328: case 336: 
					UpdateBGTileFetching();
					reg.increment_coarse_x();  // todo: they say "if rendering is enabled"
					break; 

				case 329: case 337: 
					ReloadBackgroundShiftRegisters();
					break;

				default: 
					if (!(scanline_cycle_counter & 1))
						UpdateBGTileFetching();
					break;
				}
			}
		}
		else if (current_scanline == post_render_scanline + 1) // scanline 241
		{
			if (scanline_cycle_counter == 1)
			{
				PPUSTATUS |= PPUSTATUS_vblank_mask;
				CheckNMIInterrupt();
			}
		}

		// Increment the scanline cycle counter.
		// With rendering disabled (background and sprites disabled in PPUMASK ($2001)), each scanline is 341 clocks long.
		// With rendering enabled, each odd PPU frame is one PPU cycle shorter than normal; specifically, the pre-render scanline is only 340 clocks long.
		scanline_cycle_counter = (scanline_cycle_counter + 1) % number_of_cycles_per_scanline_ntsc;
		if (scanline_cycle_counter == 0)
			PrepareForNewScanline();
	}
}


u8 PPU::ReadRegister(u16 addr)
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
	{
		// bits 4-0 are unused and then return bits 4-0 of the last value that was written to any ppu register
		u8 PPUSTATUS_ret = PPUSTATUS & 0xE0 | value_last_written_to_ppu_reg & 0x1F;
		// reading this register clears the vblank flag
		PPUSTATUS &= ~PPUSTATUS_vblank_mask;
		CheckNMIInterrupt();
		reg.w = 0;
		return PPUSTATUS_ret;
	}

	case Bus::Addr::OAMDATA: // $2004
		// during cycles 1-64, all entries of secondary OAM are initialised to 0xFF, and an internal signal makes reading from OAMDATA always return 0xFF during this time
		if (scanline_cycle_counter >= 1 && scanline_cycle_counter <= 64)
			return 0xFF;
		return OAMDATA;

	case Bus::Addr::PPUDATA  : // $2007
		// Outside of rendering, read the value and add either 1 or 32 to v.
		// During rendering, return $FF (?), and increment both coarse x and y.
		if (IsInVblank() || !RenderingIsEnabled()) 
		{
			u16 v = reg.v;
			reg.v += PPUCTRL_incr_mode ? 32 : 1;
			return ReadMemory(v);
		}
		reg.increment_coarse_x();
		reg.increment_y();
		return 0xFF;

	default: //throw std::invalid_argument(std::format("Invalid argument addr %04X given to PPU::ReadFromPPUReg", (int)addr));
		return 0xFF;
	}
}


void PPU::WriteRegister(u16 addr, u8 data)
{
	// Writes to the following registers are ignored if earlier than ~29658 CPU clocks after reset: PPUCTRL, PPUMASK, PPUSCROLL, PPUADDR. This also means that the PPUSCROLL/PPUADDR latch will not toggle
	// The other registers work immediately: PPUSTATUS, OAMADDR, OAMDATA ($2004), PPUDATA, and OAMDMA ($4014).
	
	switch (addr)
	{
	case Bus::Addr::PPUCTRL: // $2000
		//if (!cpu->all_ppu_regs_writable) return;
		PPUCTRL = value_last_written_to_ppu_reg = data;
		CheckNMIInterrupt();
		reg.t = reg.t & ~(3 << 10) | (data & 3) << 10; // Set bits 11-10 of 't' to bits 1-0 of 'data'
		return;

	case Bus::Addr::PPUMASK: // $2001
		//if (!cpu->all_ppu_regs_writable) return;
		PPUMASK = value_last_written_to_ppu_reg = data;
		return;

	case Bus::Addr::PPUSTATUS: // $2002
		value_last_written_to_ppu_reg = data;
		return; // not writable, except that bits 4-0 will be bits 4-0 of the last thing written to any ppu register (handled in read register function)

	case Bus::Addr::OAMADDR: // $2003
		OAMADDR = data;
		return;

	case Bus::Addr::OAMDATA: // $2004
		// OAM can only be written to during vertical or forced blanking
		if (PPUSTATUS_vblank || !RenderingIsEnabled())
		{
			memory.oam[OAMADDR++] = data;
		}
		else
		{
			// Do not modify values in OAM, but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits
			OAMADDR += 0b100; // todo: not sure what 'bumping only the high 6 bits' means
		}
		return;

	case Bus::Addr::PPUSCROLL: // $2005
		//if (!cpu->all_ppu_regs_writable) return;
		value_last_written_to_ppu_reg = data;
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
		//if (!cpu->all_ppu_regs_writable) return;
		value_last_written_to_ppu_reg = data;
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
		// Outside of rendering, write the value and add either 1 or 32 to v.
		// During rendering, the write is not done (?), and both coarse x and y are incremented.
		value_last_written_to_ppu_reg = data; // Todo: not 100 % if this counts as a "ppu register"
		if (this->IsInVblank() || !RenderingIsEnabled())
		{
			WriteMemory(reg.v, data);
			reg.v += PPUCTRL_incr_mode ? 32 : 1;
		}
		else
		{
			reg.increment_coarse_x();
			reg.increment_y();
		}
		return;

	case Bus::Addr::OAMDMA: // $4014
	{
		// Perform OAM DMA transfer. Writing $XX will upload 256 bytes of data from CPU page $XX00-$XXFF to the internal PPU OAM.
		// It is done by the cpu, so the cpu will stop executing instructions during this time
		// TODO: what happens if OAMDMA is written to while a transfer is already taking place?
		cpu->StartOAMDMATransfer(data, memory.oam);
	}

	}
}


void PPU::CheckNMIInterrupt()
{
	// The PPU pulls /NMI low if and only if both PPUCTRL.7 and PPUSTATUS.7 are set.
	if (PPUCTRL_NMI_enable && PPUSTATUS_vblank)
		cpu->SetNMILow();
	else
		cpu->SetNMIHigh();
}


void PPU::UpdateSpriteEvaluation()
{
	auto increment_n = [&]()
	{
		if (++sprite_evaluation.n == 64)
		{
			sprite_evaluation.idle = true;
		}
	};

	auto increment_m = [&]()
	{
		if (++sprite_evaluation.m == 4)
		{
			sprite_evaluation.m = 0;
			if (sprite_evaluation.n == 0)
				sprite_evaluation.sprite_0_included = true;
			increment_n();
			sprite_evaluation.num_sprites_copied++;
		}
	};

	if (sprite_evaluation.idle) return;

	// Fetch the next entry in OAM
	// The value of OAMADDR as it were at dot 65 is used as an offset to the address here.
	// If OAMADDR is unaligned and does not point to the y position (first byte) of an OAM entry, then whatever it points to will be reinterpreted as a y position, and the following bytes will be similarly reinterpreted.
	// When the end of OAM is reached, no more sprites will be found.
	unsigned addr = OAMADDR_at_cycle_65 + 4 * sprite_evaluation.n + sprite_evaluation.m;
	if (addr >= oam_size)
	{
		sprite_evaluation.idle = true;
		return;
	}
	u8 read_oam_entry = memory.oam[addr];

	if (sprite_evaluation.num_sprites_copied < 8)
	{
		memory.secondary_oam[sprite_evaluation.num_sprites_copied + sprite_evaluation.m] = read_oam_entry;

		if (sprite_evaluation.m == 0)
		{
			// Check if the y-pos of the current sprite ('read_oam_entry' = oam[offset + 4 * n + 0]) is in range
			if (read_oam_entry >= current_scanline && read_oam_entry < current_scanline + (PPUCTRL_sprite_height ? 16 : 8))
			{
				sprite_evaluation.m = 1;
			}
			else
			{
				sprite_evaluation.m = 0;
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
		if (read_oam_entry >= current_scanline && read_oam_entry < current_scanline + (PPUCTRL_sprite_height ? 16 : 8))
		{
			PPUSTATUS |= PPUSTATUS_sprite_overflow_mask;
			// On real hw, the ppu will continue scanning oam after setting the sprite overflow flag.
			// However, none of it will have an effect on anything other than n and m, so we may as well start idling from here.
			// Note also that the sprite overflow flag is not writeable by the cpu, and cleared only on the pre-render scanline
			sprite_evaluation.idle = true;
		}
		else
		{
			// hw bug: increment both n and m (instead of just n)
			increment_m();
			increment_n();
		}
	}
}


// Get an actual NES color (indexed 0-63) from a bg or sprite color id (0-3), given palette attribute data
u8 PPU::GetNESColorFromColorID(u8 col_id, u8 palette_attr_data, TileType tile_type)
{
	// If the color ID is 0, then the 'universal background color', located at $3F00, is always used.
	// This is equal to palette_ram[0]
	if (col_id == 0)
		return memory.palette_ram[0] & 0x3F;
	// For bg tiles, two consecutive bits of an attribute table byte holds the palette number (0-3). These have already been extracted beforehand (see the updating of the 'bg_palette_attr_reg' variable)
	// For sprites, bits 1-0 of the 'attribute byte' (byte 2 from OAM) give the palette number.
	// Each bg and sprite palette consists of three bytes (describing the actual NES colors for color ID:s 1, 2, 3), starting at $3F01, $3F05, $3F09, $3F0D respectively for bg tiles, and $3F11, $3F15, $3F19, $3F1D for sprites
	u8 palette_id = palette_attr_data & 3;
	return memory.palette_ram[1 + 4 * palette_id + 0x10 * (tile_type == TileType::OBJ)] & 0x3F;
}


void PPU::PushPixel(u8 nes_col)
{
	// From the nes colour (0-63), get an RGB24 colour from the predefined palette
	// The palette from https://wiki.nesdev.com/w/index.php?title=PPU_palettes#2C02 was used for this
	const SDL_Color& sdl_col = palette[nes_col];
	framebuffer[frame_buffer_pos    ] = sdl_col.r;
	framebuffer[frame_buffer_pos + 1] = sdl_col.g;
	framebuffer[frame_buffer_pos + 2] = sdl_col.b;
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

	if (reset_graphics_after_render)
		ResetGraphics();

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);

	gui->frames_since_update++;
}


bool PPU::RenderingIsEnabled()
{
	return PPUMASK_bg_enable || PPUMASK_sprite_enable;
}


void PPU::ResetGraphics()
{
	scale = scale_temp;
	pixel_offset_x = pixel_offset_x_temp;
	pixel_offset_y = pixel_offset_y_temp;

	SDL_RenderClear(renderer);
	reset_graphics_after_render = false;
}


void PPU::ShiftPixel()
{
	// Fetch one bit from each of the two 16-bit bg shift registers (containing pattern table data for the current tile), forming the colour id for the current bg pixel
	bool lo_bit = bg_pattern_shift_reg[0] & 1 << (15 - reg.x);
	bool hi_bit = bg_pattern_shift_reg[1] & 1 << (15 - reg.x);
	u8 bg_col_id = hi_bit << 1 | lo_bit;
	bg_pattern_shift_reg[0] <<= 1;
	bg_pattern_shift_reg[1] <<= 1;

	// Decrement the x-position counters for all 8 sprites. If a counter is 0, the sprite becomes 'active', and the shift registers for the sprite is shifted once every cycle
	// The current pixel for each 'active' sprite is checked, and the first non-transparent pixel moves on to a multiplexer, where it joins the BG pixel.
	u8 sprite_col_id = 0;
	u8 sprite_index = 0; // (0-7)
	bool non_transparent_pixel_found = false;
	for (int i = 0; i < 8; i++)
	{
		if (sprite_x_pos_counter[i] > 0)
		{
			sprite_x_pos_counter[i]--;
		}
		if (sprite_x_pos_counter[i] == 0)
		{
			if (!non_transparent_pixel_found)
			{
				bool lo_bit = sprite_pattern_shift_reg[0][i] & 0x80;
				bool hi_bit = sprite_pattern_shift_reg[1][i] & 0x80;
				u8 col_id = hi_bit << 1 | lo_bit;
				if (col_id != 0)
				{
					sprite_col_id = col_id;
					sprite_index = i;
					non_transparent_pixel_found = true;
				}
			}
			sprite_pattern_shift_reg[0][i] <<= 1;
			sprite_pattern_shift_reg[1][i] <<= 1;
		}
	}

	// Set the sprite zero hit flag if all conditions below are met
	if (!PPUSTATUS_sprite_0_hit                                                             && // The flag has not already been set this frame
	    sprite_evaluation.sprite_0_included && sprite_index == 0                            && // The current sprite is the 0th sprite in OAM
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


void PPU::ReloadBackgroundShiftRegisters()
{
	// Reload the lower 8 bits of the two 16-bit background shifters with pattern data for the next tile
	bg_pattern_shift_reg[0] = bg_pattern_shift_reg[0] & 0xFF00 | tile_fetcher.pattern_table_tile_low;
	bg_pattern_shift_reg[1] = bg_pattern_shift_reg[1] & 0xFF00 | tile_fetcher.pattern_table_tile_high;

	// For bg tiles, an attribute table byte holds palette info. Each table entry controls a 32x32 pixel metatile.
	// The byte is divided into four 2-bit areas, which each control a 16x16 pixel metatile
	// Denoting the four 16x16 pixel metatiles by 'bottomright', 'bottomleft' etc, then: value = (bottomright << 6) | (bottomleft << 4) | (topright << 2) | (topleft << 0)
	// We find which quadrant our 8x8 tile lies in. Then, the two extracted bits give the palette number (0-3) used for the tile
	bg_palette_attr_reg = tile_fetcher.attribute_table_byte >> (2 * tile_fetcher.attribute_table_quadrant) & 3;
}


void PPU::ReloadSpriteShiftRegisters()
{
	for (int sprite_index = 0; sprite_index < 8; sprite_index++)
	{
		// Reload the two 8-bit sprite shift registers (of index 'sprite_index') with pattern data for the next tile.
		// If 'sprite_index' is not less than the number of sprites copied from OAM, the registers are loaded with transparent data instead.
		if (sprite_index < sprite_evaluation.num_sprites_copied)
		{
			sprite_pattern_shift_reg[0][sprite_index] = tile_fetcher.pattern_table_tile_low;
			sprite_pattern_shift_reg[1][sprite_index] = tile_fetcher.pattern_table_tile_high;
		}
		else
		{
			sprite_pattern_shift_reg[0][sprite_index] = 0;
			sprite_pattern_shift_reg[1][sprite_index] = 0;
		}
	}
}


void PPU::UpdateBGTileFetching()
{
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
		tile_fetcher.step = TileFetcher::Step::fetch_attribute_table_byte;
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

		tile_fetcher.step = TileFetcher::Step::fetch_pattern_table_tile_low;
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_low:
	{
		/* Composition of the pattern table address for BG tiles and 8x8 sprites:
		  H RRRR CCCC P yyy
		  | |||| |||| | +++-- The row number within a tile. For BG tiles: fine Y scroll. For sprites: sprite_y_pos - fine_y_scroll
		  | |||| |||| +------ Bit plane (0: "lower"; 1: "upper")
		  | |||| ++++-------- Tile column
		  | ++++------------- Tile row
		  +------------------ Half of sprite table (0: "left"; 1: "right"); dependent on PPUCTRL flags
		  For BG tiles    : RRRR CCCC == the nametable byte fetched in step 1
		  For 8x8 sprites : RRRR CCCC == the sprite tile index number fetched from secondary OAM during cycles 257-320
		*/
		tile_fetcher.pattern_table_data_addr = (PPUCTRL_bg_tile_sel ? 0x1000 : 0x0000) | tile_fetcher.tile_num << 4 | reg.v >> 12;
		tile_fetcher.pattern_table_tile_low = ReadMemory(tile_fetcher.pattern_table_data_addr);
		tile_fetcher.step = TileFetcher::Step::fetch_pattern_table_tile_high;
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_high:
	{
		tile_fetcher.pattern_table_tile_high = ReadMemory(tile_fetcher.pattern_table_data_addr | 8);
		tile_fetcher.step = TileFetcher::Step::fetch_nametable_byte;
		break;
	}
	}
}


void PPU::UpdateSpriteTileFetching()
{
	// TODO: If there are less than 8 sprites on the next scanline, then dummy fetches to tile $FF occur for the left-over sprites, because of the dummy sprite data in the secondary OAM
	//       This data is then discarded, and the sprites are loaded with a transparent set of values instead.
	switch (tile_fetcher.step)
	{
	case TileFetcher::Step::fetch_pattern_table_tile_low:
	{
		/* Composition of the pattern table adress for 8x16 sprites:
		  H RRRR CCC S P yyy
		  | |||| ||| | | +++-- The row number within a tile: sprite_y_pos - fine_y_scroll. TODO probably not correct
		  | |||| ||| | +------ Bit plane (0: "lower"; 1: "upper")
		  | |||| ||| +-------- Sprite tile half (0: "top"; 1: "bottom") TODO: check if it isn't the reverse
		  | |||| +++---------- Tile column
		  | ++++-------------- Tile row
		  +------------------- Half of sprite table (0: "left"; 1: "right"); equal to bit 0 of the sprite tile index number fetched from secondary OAM during cycles 257-320
		  RRRR CCCC == upper 7 bits of the sprite tile index number fetched from secondary OAM during cycles 257-320
		*/
		// todo: fix; should not be "reg.v >> 12" for sprites
		if (PPUCTRL_sprite_height)  // 8x16 sprites
		{
			u8 row_num = 0; // TODO
			tile_fetcher.pattern_table_data_addr = (tile_fetcher.tile_num & 1) << 12 | (tile_fetcher.tile_num & ~1) << 4 | reg.v >> 12;
			// Check if we are on the top or bottom tile of the 8x16 sprite
			if (reg.v & 0x20) // i.e. the course Y scroll is odd, meaning that we are on the bottom tile
				tile_fetcher.pattern_table_data_addr |= 0x10;
		}
		else // 8x8 sprites
		{
			u8 row_num = tile_fetcher.y_pos - (reg.v >> 12); // 0-7
			tile_fetcher.pattern_table_data_addr = (PPUCTRL_sprite_tile_sel ? 0x1000 : 0x0000) | tile_fetcher.tile_num << 4 | row_num;
		}

		tile_fetcher.pattern_table_tile_low = ReadMemory(tile_fetcher.pattern_table_data_addr);
		tile_fetcher.step = TileFetcher::Step::fetch_pattern_table_tile_high;
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_high:
	{
		tile_fetcher.pattern_table_tile_high = ReadMemory(tile_fetcher.pattern_table_data_addr | 8);
		tile_fetcher.step = TileFetcher::Step::fetch_pattern_table_tile_low;
		break;
	}
	}
}


// Reading and writing done internally by the ppu
u8 PPU::ReadMemory(u16 addr)
{
	// $0000-$1FFF - Pattern tables; maps to CHR ROM/RAM on the game cartridge
	if (addr <= 0x1FFF)
	{
		return mapper->ReadCHR(addr);
	}
	// $2000-$2FFF - Nametables; internal ppu vram. $3000-$3EFF - mirror of $2000-$2EFF
	else if (addr <= 0x3EFF)
	{
		return memory.vram[addr & 0xFFF];
	}
	// $3F00-$3F1F - Palette RAM indeces. $3F20-$3FFF - mirrors of $3F00-$3F1F
	else if (addr <= 0x3FFF)
	{
		// Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
		// Note: bits 4-0 of all mirrors have the form 1xy00, and the redirected addresses have the form 0xy00
		if ((addr & 0b10011) == 0b10000)
			addr -= 0x10;
		return memory.palette_ram[addr & 0x1F];
	}
	else
	{
		// throw exception
		return 0xFF;
	}
}


void PPU::WriteMemory(u16 addr, u8 data)
{
	// $0000-$1FFF - Pattern tables; maps to CHR ROM/RAM on the game cartridge
	if (addr <= 0x1FFF)
	{
		mapper->WriteCHR(addr, data);
	}
	// $2000-$2FFF - Nametables; internal ppu vram. $3000-$3EFF - mirror of $2000-$2EFF
	else if (addr <= 0x3EFF)
	{
		memory.vram[addr & 0xFFF] = data;
	}
	// $3F00-$3F1F - Palette RAM indeces. $3F20-$3FFF - mirrors of $3F00-$3F1F
	else if (addr <= 0x3FFF)
	{
		// Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
		// Note: bits 4-0 of all mirrors have the form 1xy00, and the redirected addresses have the form 0xy00
		if ((addr & 0b10011) == 0b10000)
			addr -= 0x10;
		memory.palette_ram[addr & 0x1F] = data;
	}
}


void PPU::PrepareForNewFrame()
{
	odd_frame = !odd_frame;
	frame_buffer_pos = 0;
}


void PPU::PrepareForNewScanline()
{
	current_scanline = (current_scanline + 1) % number_of_scanlines_ntsc;
	if (current_scanline == 0)
		PrepareForNewFrame();
	pixel_x_pos = 0;
}


void PPU::SetWindowSize(wxSize size)
{
	unsigned width = size.GetWidth(), height = size.GetHeight();

	if (width > 0 && height > 0)
	{
		scale_temp = std::min(width / resolution_x, height / resolution_y);
		pixel_offset_x_temp = 0.5 * (width - scale_temp * resolution_x);
		pixel_offset_y_temp = 0.5 * (height - scale_temp * resolution_y);
		reset_graphics_after_render = true;
	}
}


void PPU::Configure(Serialization::BaseFunctor& functor)
{
	functor.fun(&scale, sizeof(unsigned));
}


void PPU::SetDefaultConfig()
{
	scale = default_scale;
}


void PPU::LogState()
{
	Logging::ReportPpuState(current_scanline, scanline_cycle_counter);
}