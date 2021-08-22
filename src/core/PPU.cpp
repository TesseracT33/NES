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
				PPUSTATUS &= ~(PPUSTATUS_vblank_mask | PPUSTATUS_sprite_overflow_mask);
			}
		}
		else if (current_scanline < post_render_scanline) // < 240
		{
			if (ppu_cycle_counter == 1)
			{
				// Clear secondary OAM. Is supposed to happen one write at a time between cycles 1-64 (each write taking two cycles).
				// However, can all be done here, as secondary OAM can is not accessed from elsewhere during this time (?)
				for (int i = 0; i < 32; i++)
					secondary_oam[i] = 0xFF;
			}
			else if (ppu_cycle_counter <= 256)
			{
				UpdateTileFetcher();
				if (!(ppu_cycle_counter & 1) && (PPUMASK_sprite_enable || PPUMASK_bg_enable))
					DoSpriteEvaluation();
			}
			else if (ppu_cycle_counter <= 320)
			{

			}
			else if (ppu_cycle_counter <= 336)
			{
				UpdateTileFetcher();
			}
			else
			{

			}

			if (reload_bg_shifters)
			{
				// this way of doing things may not make any sense... we will see
				bg_pattern_table_shift_reg[0] = bg_pattern_table_shift_reg[0] & 0xFF | tile_fetcher.pattern_table_tile_low << 8;
				bg_pattern_table_shift_reg[1] = bg_pattern_table_shift_reg[1] & 0xFF | tile_fetcher.pattern_table_tile_high << 8;
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
	case Bus::Addr::PPUDATA  : return memory.Read(ppuaddr_written_addr);

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
			memory.Write(ppuaddr_written_addr, data);
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


void PPU::ShiftPixel()
{

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
			if (read_oam >= current_scanline && read_oam < current_scanline + 8)  // todo: sprites with height 16
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
		if (read_oam >= current_scanline && read_oam < current_scanline + 8)  // todo: sprites with height 16
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

	// on every call to this function, alternate between updating and sleeping, since each operation takes two cycles
	if (tile_fetcher.sleep_on_next_update)
	{
		tile_fetcher.step = static_cast<TileFetcher::Step>((tile_fetcher.step + 1) % 4);
	}
	else
	{
		switch (tile_fetcher.step)
		{

		case TileFetcher::Step::fetch_nametable_byte:
		{
			u16 base_addr = 0x2000 + 0x400 * PPUCTRL_nametable_sel;
			u16 index = 32 * (current_scanline / 8) + tile_fetcher.x_pos;
			tile_fetcher.nametable_byte = memory.Read(base_addr + index);
			break;
		}

		case TileFetcher::Step::fetch_attribute_table_byte:
		{
			u16 base_addr = 0x23C0 + 0x400 * PPUCTRL_nametable_sel;
			u8 index = 8 * (current_scanline / 32) + tile_fetcher.x_pos / 2;
			tile_fetcher.attribute_table_byte = memory.Read(base_addr + index);
			break;
		}

		case TileFetcher::Step::fetch_pattern_table_tile_low:
		{
			u16 base_addr = GetBasePatternTableIndex(); // either 0x0000 or 0x1000, depending on if the right or left pattern table is used
			u16 index = tile_fetcher.nametable_byte << 1 | (current_scanline % 8);
			tile_fetcher.pattern_table_tile_low = memory.Read(base_addr + index);
			break;
		}

		case TileFetcher::Step::fetch_pattern_table_tile_high:
		{
			u16 base_addr = GetBasePatternTableIndex();
			u16 index = tile_fetcher.nametable_byte << 1 | (current_scanline % 8 + 8);
			tile_fetcher.pattern_table_tile_high = memory.Read(base_addr + index);
			reload_bg_shifters = true;
			break;
		}

		}
	}

	tile_fetcher.sleep_on_next_update = !tile_fetcher.sleep_on_next_update;
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