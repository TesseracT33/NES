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
		if (current_scanline == pre_render_scanline) // == -1
		{
			if (ppu_cycle_counter == 1)
			{
				PPUSTATUS &= ~(PPUSTATUS_vblank_mask | PPUSTATUS_sprite_0_hit_mask | PPUSTATUS_sprite_overflow_mask);
			}
		}
		else if (current_scanline < post_render_scanline) // < 240
		{
			if (ppu_cycle_counter <= 256)
			{
				if (ppu_cycle_counter == 0)
				{
					continue;
				}
				if (ppu_cycle_counter == 1)
				{
					// Clear secondary OAM. Is supposed to happen one write at a time between cycles 1-64 (each write taking two cycles).
					// However, can all be done here, as secondary OAM can is not accessed from elsewhere during this time (?)
					for (int i = 0; i < 32; i++)
						secondary_oam[i] = 0xFF;

					tile_fetcher.tile_type = TileFetcher::TileType::BG;
				}

				if (ppu_cycle_counter & 1)
					UpdateTileFetcher();
				else if (PPUMASK_sprite_enable || PPUMASK_bg_enable)
					DoSpriteEvaluation();
			}
			else if (ppu_cycle_counter <= 320)
			{
				if (ppu_cycle_counter == 257)
				{
					tile_fetcher.tile_type = TileFetcher::TileType::OAM_8x8;
				}
				if (ppu_cycle_counter & 1)
					UpdateTileFetcher();
				if ((ppu_cycle_counter - 257) % 8 == 0)
					FetchSpriteDataFromSecondaryOAM();
				else if ((ppu_cycle_counter - 257) % 8 == 2)
				{
					u8 sprite_index = tile_fetcher.x_pos;
					sprite_attribute_latch[sprite_index] = secondary_oam[sprite_index + 2];
				}
				else if ((ppu_cycle_counter - 257) % 8 == 3)
				{
					u8 sprite_index = tile_fetcher.x_pos;
					sprite_x_pos_counter[sprite_index] = secondary_oam[sprite_index + 3];
				}
			}
			else // <= 340 (or 341, for even-numbered frames). todo: UpdateTileFetcher will be called on cycle 341, is it correct?
			{
				if (ppu_cycle_counter & 1)
					UpdateTileFetcher();
			}

			// On cycles 9, 17, ..., 257, as well as 329 and 337, the bg shift registers is fed with new tile data
			if (ppu_cycle_counter >= 9 && ppu_cycle_counter <= 257 && ppu_cycle_counter % 8 == 1 || 
				ppu_cycle_counter == 329 || ppu_cycle_counter == 337)
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

			if (current_scanline > pre_render_scanline && current_scanline < post_render_scanline)
			{
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
	}
}


u8 PPU::ReadFromPPUReg(u16 addr)
{
	switch (addr)
	{
	case Bus::Addr::PPUCTRL  :
	case Bus::Addr::PPUMASK  :
	case Bus::Addr::PPUSCROLL:
	case Bus::Addr::OAMADDR  :  
	case Bus::Addr::OAMDMA   : return 0xFF; // write-only. TODO: return 0xFF or 0?

	case Bus::Addr::PPUSTATUS: 
		PPUSTATUS &= ~PPUSTATUS_vblank_mask;
		return PPUSTATUS;

	case Bus::Addr::PPUADDR  : return PPUADDR; // todo: should it return the last written byte?
	case Bus::Addr::PPUDATA  : return ReadMemory(ppuaddr_written_addr);

	case Bus::Addr::OAMDATA:
		// during cycles 1-64, secondary OAM is initialised to 0xFF, and an internal signal makes reading from OAMDATA always return 0xFF during this time
		if (ppu_cycle_counter >= 1 && ppu_cycle_counter <= 64)
			return 0xFF;
		return OAMDATA;

	default: //throw std::invalid_argument(std::format("Invalid argument addr %04X given to PPU::ReadFromPPUReg", (int)addr));
		;
	}
}


void PPU::WriteToPPUReg(u16 addr, u8 data)
{
	switch (addr)
	{
	case Bus::Addr::PPUCTRL:
		if (!PPUCTRL_NMI_enable && (data & PPUCTRL_NMI_enable_mask) && PPUSTATUS_vblank && this->IsInVblank())
		{
			// todo: generate NMI signal
		}
		PPUCTRL = data;
		// todo: After power/reset, writes to this register are ignored for about 30,000 cycles.
		return;

	case Bus::Addr::PPUMASK:
		PPUMASK = data;
		return;

	case Bus::Addr::PPUSTATUS:
		PPUSTATUS = data;
		return;

	case Bus::Addr::OAMADDR:
		OAMADDR = data;
		return;

	case Bus::Addr::OAMDATA:
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

	case Bus::Addr::PPUSCROLL:
		if (!ppuscroll_written_to)
			scroll_x = data;
		else
			scroll_y = (data < 240) ? data : (data - 256); // written values of 240 to 255 are treated as -16 through -1
		ppuscroll_written_to = !ppuscroll_written_to;
		return;

	case Bus::Addr::PPUADDR:
		if (ppuaddr_written_to)
			ppuaddr_written_addr = (PPUADDR << 8 | data) & 0x3FFF; // Valid addresses are $0000-$3FFF
		PPUADDR = data; // todo: correct?
		ppuaddr_written_to = !ppuaddr_written_to;
		return;

	case Bus::Addr::PPUDATA:
		if (this->IsInVblank() || (!PPUMASK_bg_enable && !PPUMASK_sprite_enable))
		{
			WriteMemory(ppuaddr_written_addr, data);
			ppuaddr_written_addr += PPUCTRL_incr_mode ? 32 : 1; // todo:  (0: add 1, going across; 1: add 32, going down), meaning what?
		}
		return;

	case Bus::Addr::OAMDMA:
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


void PPU::UpdateTileFetcher()
{
	auto GetBasePatternTableIndex = [&]()
	{
		switch (tile_fetcher.tile_type)
		{
		case TileFetcher::TileType::BG: return PPUCTRL_bg_tile_sel ? 0x1000 : 0x0000;
		case TileFetcher::TileType::OAM_8x8: return PPUCTRL_sprite_tile_sel ? 0x1000 : 0x0000;
		case TileFetcher::TileType::OAM_8x16: return 0; // todo:  bit 0 of each OAM entry's tile number applies to 8x16 sprites.
		}
	};

	switch (tile_fetcher.step)
	{
	case TileFetcher::Step::fetch_nametable_byte:
	{
		u16 base_addr = 0x2000 + 0x400 * PPUCTRL_nametable_sel;
		u16 index = 32 * (current_scanline / 8) + tile_fetcher.x_pos;
		tile_fetcher.nametable_byte = ReadMemory(base_addr + index);
		break;
	}

	case TileFetcher::Step::fetch_attribute_table_byte:
	{
		u16 base_addr = 0x23C0 + 0x400 * PPUCTRL_nametable_sel;
		u8 index = 8 * (current_scanline / 32) + tile_fetcher.x_pos / 4;
		tile_fetcher.attribute_table_byte = ReadMemory(base_addr + index);
		tile_fetcher.attribute_table_quadrant = 2 * (current_scanline % 32 > 15) + (tile_fetcher.x_pos % 4 > 1);
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_low:
	{
		u16 base_addr = GetBasePatternTableIndex(); // either 0x0000 or 0x1000, depending on if the right or left pattern table is used
		u16 index;
		if (tile_fetcher.tile_type == TileFetcher::TileType::BG)
			index = tile_fetcher.nametable_byte << 1 | (current_scanline % 8);
		else
			index = sprites.pattern_data[(ppu_cycle_counter - 257) / 8] << 1 | (current_scanline % 8);
		tile_fetcher.pattern_table_tile_low = ReadMemory(base_addr + index);
		break;
	}

	case TileFetcher::Step::fetch_pattern_table_tile_high:
	{
		u16 base_addr = GetBasePatternTableIndex();
		u16 index;
		if (tile_fetcher.tile_type == TileFetcher::TileType::BG)
			index = tile_fetcher.nametable_byte << 1 | (current_scanline % 8 + 8);
		else
			index = sprites.pattern_data[(ppu_cycle_counter - 257) / 8] << 1 | (current_scanline % 8 + 8);
		tile_fetcher.pattern_table_tile_high = ReadMemory(base_addr + index);
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