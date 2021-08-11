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
	for (int i = 0; i < 3; i++) // PPU::Update() is called once each cpu cycle, but 1 cpu cycle = 3 ppu cycles
	{
		if (ppu_cycle_counter == 1)
		{
			if (current_scanline == pre_render_scanline)
				PPUSTATUS &= ~PPUSTATUS_vblank_mask;
			else if (current_scanline == post_render_scanline)
				PPUSTATUS |= PPUSTATUS_vblank_mask;
		}

		if (ppu_cycle_counter == 0)
		{

		}
		else if (ppu_cycle_counter <= 256)
		{
			FetchTile();
		}
		else if (ppu_cycle_counter <= 320)
		{

		}
		else if (ppu_cycle_counter <= 336)
		{

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
	case Bus::Addr::OAMDATA  : return OAMDATA;
	}
}


void PPU::WriteToPPUReg(u16 addr, u8 data)
{
	switch (addr)
	{
	case Bus::Addr::PPUCTRL:
		if (PPUCTRL_NMI_enable && PPUCTRL_NMI_enable && PPUSTATUS_vblank && this->IsInVblank())
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
			// todo: perform a glitchy increment of OAMADDR, bumping only the high 6 bits
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
		// perform OAM DMA transfer
		const u16 start_addr = OAMADDR << 8;
		for (u16 addr = start_addr; addr <= start_addr + 0xFF; addr++)
			memory.oam[addr & 0xFF] = bus->Read(addr);
		// todo: stop cpu from executing instructions for a while
	}

	}
}


void PPU::FetchTile()
{
	static u8 nametable_byte, attribute_table_byte, pattern_table_tile_low, pattern_table_tile_high;

	if (tile_fetcher.cycle == 1)
	{
		tile_fetcher.cycle = 0;
		tile_fetcher.step = (tile_fetcher.step + 1) % 4;
	}
	else
	{
		switch (tile_fetcher.step)
		{

		case TileFetcher::nametable_byte:
		{
			u16 index = 32 * (current_scanline / 8) + tile_fetcher.x_pos; // todo: current_scanline == -1?
			nametable_byte = memory.vram[index];
			break;
		}

		case TileFetcher::attribute_table_byte:
		{

			break;
		}

		case TileFetcher::pattern_table_tile_low:
		{

			break;
		}

		}
	}
}


void PPU::PrepareForNewFrame()
{
	odd_frame = !odd_frame;
}


void PPU::PrepareForNewScanline()
{
	current_scanline = (current_scanline + 1) % 262;
	if (current_scanline == 0)
		PrepareForNewFrame();
}