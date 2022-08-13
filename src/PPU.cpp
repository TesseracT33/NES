module PPU;

import Bus;
import Cartridge;
import CPU;
import System;

import Util.Bit;

import Video;

namespace PPU
{
	uint GetFrameBufferSize() 
	{ 
		return num_pixels_per_scanline * System::standard.num_visible_scanlines * num_colour_channels;
	}


	bool InVblank()
	{
		/* Note: vblank is counted to begin on the first "post-render" scanline, not on the same scanline as when NMI is triggered. */
		return scanline >= System::standard.nmi_scanline - 1;
	}


	void PowerOn()
	{
		static constexpr std::array<u8, 0x20> palette_ram_on_powerup = { /* Source: blargg_ppu_tests_2005.09.15b */
			0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C,
			0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08
		};

		Reset();
		ppustatus = std::bit_cast<decltype(ppustatus), u8>(u8(0));
		oamaddr = scroll.v = scroll.t = a12 = 0;
		nmi_line = 1;
		palette_ram = palette_ram_on_powerup;
		framebuffer.resize(GetFrameBufferSize());

		Video::SetFramebufferPtr(framebuffer.data());
		Video::SetFramebufferSize(num_pixels_per_scanline, System::standard.num_visible_scanlines);
		Video::SetPixelFormat(Video::PixelFormat::RGB888);
	}


	void Reset()
	{
		ppuctrl = std::bit_cast<decltype(ppuctrl), u8>(u8(0));
		ppumask = std::bit_cast<decltype(ppumask), u8>(u8(0));
		ppuscroll = ppudata = scroll.w = 0;
		scanline_cycle = scanline = pixel_x_pos = framebuffer_pos = 0;
		odd_frame = true;
		rendering_is_enabled = false;
	}


	void Update()
	{
		/* Update() is called once each cpu cycle.
		   On NTSC/Dendy: 1 cpu cycle = 3 ppu cycles.
		   On PAL       : 1 cpu cycle = 3.2 ppu cycles. */
		if (System::standard.ppu_dots_per_cpu_cycle == 3) { /* NTSC/Dendy */
			StepCycle();
			StepCycle();
			// The NMI edge detector and IRQ level detector is polled during the second half of each cpu cycle. Here, we are polling 2/3 in.
			CPU::PollInterruptInputs();
			StepCycle();
			/* Updated on a per-cpu-cycle basis, as precision isn't very important here. */
			open_bus_io.UpdateDecay(3 /* elapsed ppu cycles */);
		}
		else { /* PAL */
			StepCycle();
			StepCycle();
			CPU::PollInterruptInputs();
			StepCycle();
			if (++cpu_cycle_counter == 5) {
				/* This makes for a total of 3 * 5 + 1 = 16 = 3.2 * 5 ppu cycles per every 5 cpu cycles. */
				StepCycle();
				cpu_cycle_counter = 0;
				open_bus_io.UpdateDecay(4);
			}
			else {
				open_bus_io.UpdateDecay(3);
			}
		}
		if (cpu_cycles_since_a12_set_low < 3 && a12 == 0) {
			cpu_cycles_since_a12_set_low++;
		}
	}


	void StepCycle()
	{
		if (set_sprite_0_hit_flag && scanline_cycle >= 2) {
			ppustatus.sprite_0_hit = 1;
			set_sprite_0_hit_flag = false;
		}
		if (scanline_cycle == 0) {
			// Idle cycle on every scanline, except for if cycle 340 on the previous scanline was skipped. Then, we perform another dummy nametable fetch.
			if (cycle_340_was_skipped_on_last_scanline) {
				if (rendering_is_enabled) {
					UpdateBGTileFetching();
				}
				cycle_340_was_skipped_on_last_scanline = false;
			}
			scanline_cycle = 1;
			tile_fetcher.StartOver();
			return;
		}

		/* NTSC     : scanlines -1 (pre-render), 0-239
		*  PAL/Dendy: scanlines -1 (pre-render), 0-238 */
		if (scanline < System::standard.num_visible_scanlines) {
			if (scanline_cycle <= 256) { // Cycles 1-256
				// The shifters are reloaded during ticks 9, 17, 25, ..., 257, i.e., if tile_fetch_cycle_step == 0 && scanline_cycle >= 9
				// They are only reloaded on visible scanlines.
				if (tile_fetcher.cycle_step == 0 && scanline_cycle >= 9 && scanline != pre_render_scanline) {
					ReloadBackgroundShiftRegisters();
				}
				// Update the BG tile fetching every cycle (if rendering is enabled).s
				// Although no pixels are rendered on the pre-render scanline, the PPU still makes the same memory accesses it would for a regular scanline.
				if (rendering_is_enabled) {
					UpdateBGTileFetching();
				}
				// Shift one pixel per cycle during cycles 1-256 on visible scanlines
				// Sprite evaluation happens either if bg or sprite rendering is enabled, but not on the pre render scanline (oddly enough)
				// If one the pre render scanline, clear ppu status flags and render graphics on dot 1
				if (scanline == pre_render_scanline) {
					if (scanline_cycle == 1) {
						ppustatus.sprite_0_hit = ppustatus.sprite_overflow = ppustatus.vblank = 0;
						CheckNMI();
						Video::RenderGame();
					}
				}
				else {
					if (rendering_is_enabled) {
						UpdateSpriteEvaluation();
					}
					ShiftPixel();
				}
			}
			else if (scanline_cycle <= 320) { // Cycles 257-320
				// OAMADDR is set to 0 at every cycle in this interval on visible scanlines + on the pre-render one (if rendering is enabled)
				if (rendering_is_enabled) {
					oamaddr = 0;
				}
				if (scanline_cycle == 257) {
					ReloadBackgroundShiftRegisters(); // Update the bg shift registers at cycle 257
					if (rendering_is_enabled) {
						scroll.v = scroll.v & ~0x41F | scroll.t & 0x41F; // copy all bits related to horizontal position from t to v:
					}
					secondary_oam_sprite_index = 0;
				}
				// Consider an 8 cycle period (0-7) between cycles 257-320 (of which there are eight: one for each sprite)
				// On cycle 0-3: read the Y-coordinate, tile number, attributes, and X-coordinate of the selected sprite from secondary OAM.
				//    Note: All of this can be done on cycle 0, as none of this data is used until cycle 5 at the earliest (some of it is not used until the next scanline).
				// On each cycle, update the sprite tile fetching.
				// On cycle 8 (i.e. the cycle after each period: 265, 273, ..., 321), update the sprite shift registers with pattern data.
				if (rendering_is_enabled) {
					if (tile_fetcher.cycle_step == 0) {
						tile_fetcher.sprite_y_pos                          = secondary_oam[4 * secondary_oam_sprite_index];
						tile_fetcher.tile_num                              = secondary_oam[4 * secondary_oam_sprite_index + 1];
						tile_fetcher.sprite_attr                           = secondary_oam[4 * secondary_oam_sprite_index + 2];
						sprite_attribute_latch[secondary_oam_sprite_index] = secondary_oam[4 * secondary_oam_sprite_index + 2];
						sprite_x_pos_counter[secondary_oam_sprite_index]   = secondary_oam[4 * secondary_oam_sprite_index + 3];
						if (scanline_cycle >= 265) {
							ReloadSpriteShiftRegisters(secondary_oam_sprite_index - 1); // Once we've hit this point for the first time, it's time to update for sprite 0, but sprite_index will be 1.
						}
						secondary_oam_sprite_index++;
					}
					UpdateSpriteTileFetching();
					if (scanline == pre_render_scanline && scanline_cycle >= 280 && scanline_cycle <= 304) {
						// Copy the vertical bits of t to v
						scroll.v = scroll.v & ~0x7BE0 | scroll.t & 0x7BE0;
					}
				}
			}
			else { // Cycles 321-340
				// Reload the shift registers for the 7th and last sprite.
				if (scanline_cycle == 321) {
					ReloadSpriteShiftRegisters(7);
				}
				// Between cycles 322 and 337, the background shift registers are shifted.
				else if (scanline_cycle <= 337) {
					bg_pattern_shift_reg[0] <<= 1;
					bg_pattern_shift_reg[1] <<= 1;
					bg_palette_attr_reg[0] <<= 1;
					bg_palette_attr_reg[1] <<= 1;
				}
				// Reload the BG shift registers at cycle 329 and 337 (for a total of two tiles fetched)
				if (scanline_cycle == 329 || scanline_cycle == 337) {
					ReloadBackgroundShiftRegisters();
				}
				// Update BG tile fetching during each cycle. In total, two tiles are fetched + two nametable fetches.
				if (rendering_is_enabled) {
					UpdateBGTileFetching();
				}
			}
		}
		/* NTSC: scanline 241. PAL: scanline 240. Dendy: scanline 290 */
		else if (scanline == System::standard.nmi_scanline && scanline_cycle == 1) {
			ppustatus.vblank = 1;
			CheckNMI();
			SetA12(scroll.v & 0x1000); /* At the start of vblank, the bus address is set back to the video ram address. */
			scanline_cycle = 2;
			return;
		}

		// Increment the scanline cycle counter. Normally, each scanline is 341 clocks long.
		// On NTSC specifically:
		//   With rendering enabled, each odd PPU frame is one PPU cycle shorter than normal; specifically, the pre-render scanline is only 340 clocks long.
		//   The last nametable fetch, normally taking place on cycle 340, then takes place on cycle 0 the following scanline.
		if (scanline_cycle == 339) {
			if (System::standard.pre_render_line_is_one_dot_shorter_on_every_other_frame &&
				scanline == pre_render_scanline && odd_frame && rendering_is_enabled)
			{
				scanline_cycle = 0;
				cycle_340_was_skipped_on_last_scanline = true;
				PrepareForNewScanline();
			}
			else {
				scanline_cycle = 340;
			}
		}
		else if (scanline_cycle == 340) {
			scanline_cycle = 0;
			PrepareForNewScanline();
		}
		else {
			scanline_cycle++;
		}
	}


	u8 ReadRegister(u16 addr)
	{
		/* The following shows the effect of a read from each register:
		Addr    Open-bus bits
				7654 3210
		-----------------
		$2000   DDDD DDDD
		$2001   DDDD DDDD
		$2002   ---D DDDD
		$2003   DDDD DDDD
		$2004   ---- ----
		$2005   DDDD DDDD
		$2006   DDDD DDDD
		$2007   ---- ----   non-palette
				DD-- ----   palette

		A D means that this bit reads back as whatever is in the decay register
		at that bit, and doesn't refresh the decay register at that bit. A -
		means that this bit reads back as defined by the PPU, and refreshes the
		decay register at the corresponding bit. */

		switch (addr & 7) {
		case 0: // $2000; PPUCTRL (write-only)
		case 1: // $2001; PPUMASK (write-only)
		case 3: // $2003; OAMADDR (write-only)
		case 5: // $2005; PPUSCROLL (write-only)
		case 6: // $2006; PPUADDR (write-only)
			return open_bus_io.Read();

		case 2: { // $2002; PPUSTATUS (read-only)
			u8 ppustatus_u8 = std::bit_cast<u8, decltype(ppustatus)>(ppustatus);
			u8 ret = ppustatus_u8 & 0xE0 | open_bus_io.Read(0x1F); /* Bits 4-0 are unused and then return bits 4-0 of open bus */
			open_bus_io.UpdateValue(ppustatus_u8, 0xE0); /* Update bits 7-5 of open bus with the read value */
			ppustatus.vblank = 0; /* Reading this register clears the vblank flag */
			CheckNMI();
			scroll.w = 0;
			return ret;
		}

		case 4: { // $2004; OAMDATA (read/write)
			// TODO: according to nesdev: during cycles 1-64, all entries of secondary OAM are initialised to 0xFF, and an internal signal makes reading from OAMDATA always return 0xFF during this time
			// Is this actually true? blargg 'ppu_open_bus' and 'sprite_ram' tests fail if this is emulated, and Mesen does not seem to implement it.
			u8 ret = oam[oamaddr];
			// Bits 2-4 of sprite attributes should always be clear when read (these are unimplemented).
			if ((oamaddr & 3) == 2) {
				ret &= 0xE3;
			}
			open_bus_io.UpdateValue(ret, 0xFF); /* Update all bits of open bus with the read value */
			return ret;
		}

		case 7: { // $2007; PPUDATA (read/write)
			// Outside of rendering, read the value at address 'v' and add either 1 or 32 to 'v'.
			// During rendering, return $FF (?), and increment both coarse x and y.
			if (InVblank() || !rendering_is_enabled) {
				u8 ret;
				u16 v_read = scroll.v & 0x3FFF; // Only bits 0-13 of v are used; the PPU memory space is 14 bits wide.
				// When reading while the VRAM address is in the range 0-$3EFF (i.e., before the palettes), the read will return the contents of an internal read buffer which is updated only when reading PPUDATA.
				// After the CPU reads and gets the contents of the internal buffer, the PPU will immediately update the internal buffer with the byte at the current VRAM address.
				if (v_read <= 0x3EFF) {
					ret = ppudata;
					ppudata = ReadMemory(v_read);
					open_bus_io.UpdateValue(ret, 0xFF); /* Update all bits of open bus with the read value */
				}
				// When reading palette data $3F00-$3FFF, the palette data is placed immediately on the data bus.
				// However, reading the palettes still updates the internal buffer, but the data is taken from a section of mirrored nametable data.
				else {
					// High 2 bits from palette should be from open bus. Reading palette shouldn't refresh high 2 bits of open bus.
					ret = ReadPaletteRAM(v_read) | open_bus_io.Read(0xC0); // Note: the result from ReadPaletteRAM is guaranteed to have bits 7-6 cleared.
					ppudata = ReadMemory(v_read & 0xFFF | 0x2000); // Read from vram at $2000-$2FFF
					open_bus_io.UpdateValue(ret, 0x3F); /* Update bits 5-0 of open bus with the read value */
				}
				scroll.v += (ppuctrl.incr_mode ? 32 : 1);
				SetA12(scroll.v & 0x1000);
				return ret;
			}
			else {
				scroll.IncrementCoarseX();
				scroll.IncrementFineY();
				return open_bus_io.Read();
			}
		}

		default:
			std::unreachable();
		}
	}


	u8 PeekRegister(u16 addr)
	{
		switch (addr & 7) {
		case 0: // $2000; PPUCTRL (write-only)
		case 1: // $2001; PPUMASK (write-only)
		case 3: // $2003; OAMADDR (write-only)
		case 5: // $2005; PPUSCROLL (write-only)
		case 6: // $2006; PPUADDR (write-only)
			return open_bus_io.Read();

		case 2: { // $2002; PPUSTATUS (read-only)
			u8 ppustatus_u8 = std::bit_cast<u8, decltype(ppustatus)>(ppustatus);
			return ppustatus_u8 & 0xE0 | open_bus_io.Read(0x1F); /* Bits 4-0 are unused and then return bits 4-0 of open bus */
		}

		case 4: { // $2004; OAMDATA (read/write)
			// TODO: according to nesdev: during cycles 1-64, all entries of secondary OAM are initialised to 0xFF, and an internal signal makes reading from OAMDATA always return 0xFF during this time
			// Is this actually true? blargg 'ppu_open_bus' and 'sprite_ram' tests fail if this is emulated, and Mesen does not seem to implement it.
			u8 ret = oam[oamaddr];
			// Bits 2-4 of sprite attributes should always be clear when read (these are unimplemented).
			if ((oamaddr & 3) == 2) {
				ret &= 0xE3;
			}
			return ret;
		}

		case 7: { // $2007; PPUDATA (read/write)
			// Outside of rendering, read the value at address 'v' and add either 1 or 32 to 'v'.
			// During rendering, return $FF (?), and increment both coarse x and y.
			if (InVblank() || !rendering_is_enabled) {
				u16 v_read = scroll.v & 0x3FFF; // Only bits 0-13 of v are used; the PPU memory space is 14 bits wide.
				// When reading while the VRAM address is in the range 0-$3EFF (i.e., before the palettes), the read will return the contents of an internal read buffer which is updated only when reading PPUDATA.
				// After the CPU reads and gets the contents of the internal buffer, the PPU will immediately update the internal buffer with the byte at the current VRAM address.
				if (v_read <= 0x3EFF) {
					return ppudata;
				}
				// When reading palette data $3F00-$3FFF, the palette data is placed immediately on the data bus.
				// However, reading the palettes still updates the internal buffer, but the data is taken from a section of mirrored nametable data.
				else {
					// High 2 bits from palette should be from open bus. Reading palette shouldn't refresh high 2 bits of open bus.
					return ReadPaletteRAM(v_read) | open_bus_io.Read(0xC0); // Note: the result from ReadPaletteRAM is guaranteed to have bits 7-6 cleared.
				}
			}
			else {
				return open_bus_io.Read();
			}
		}

		default:
			std::unreachable();
		}
	}


	u8 ReadOAMDMA()
	{
		return open_bus_io.Read();
	}


	u8 PeekOAMDMA()
	{
		return open_bus_io.Read();
	}


	void WriteRegister(u16 addr, u8 data)
	{
		/* Writes to any PPU port, including the nominally read-only status port at $2002, load a value onto the entire PPU's I/O bus */
		open_bus_io.Write(data);

		switch (addr & 7) {
		case 0: // $2000; PPUCTRL (write-only)
			ppuctrl = std::bit_cast<decltype(ppuctrl), u8>(data);
			CheckNMI();
			scroll.t = scroll.t & ~0xC00 | (data & 3) << 10; // Set bits 11-10 of 't' to bits 1-0 of 'data'
			break;

		case 1: // $2001; PPUMASK (write-only)
			ppumask = std::bit_cast<decltype(ppumask), u8>(data);
			rendering_is_enabled = ppumask.bg_enable || ppumask.sprite_enable;
			break;

		case 2: // $2002; PPUSTATUS (read-only)
			break;

		case 3: // $2003; OAMADDR (write-only)
			oamaddr = data;
			break;

		case 4: // $2004; OAMDATA (read/write)
			// On NTSC/Dendy: OAM can only be written to during vertical (up to 20 scanlines after NMI) or forced blanking.
			// On PAL: OAM can only be written to during the first 20 scanlines after NMI
			if (scanline < System::standard.nmi_scanline + 20 ||
				System::standard.oam_can_be_written_to_during_forced_blanking && !rendering_is_enabled)
			{
				oam[oamaddr++] = data;
			}
			else {
				// Do not modify values in OAM, but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits
				oamaddr += 0b100;
			}
			break;

		case 5: // $2005; PPUSCROLL (write-only)
			if (scroll.w == 0) { // Update x-scroll registers
				scroll.t = scroll.t & ~0x1F | data >> 3; // Set bits 4-0 of 't' (coarse x-scroll) to bits 7-3 of 'data'
				scroll.x = data; // Set 'x' (fine x-scroll) to bits 2-0 of 'data'
			}
			else { // Update y-scroll registers
				// Set bits 14-12 of 't' (fine y-scroll) to bits 2-0 of 'data', and bits 9-5 of 't' (coarse y-scroll) to bits 7-3 of 'data'
				scroll.t = scroll.t & ~0x73E0 | (data & 0x07) << 12 | (data & 0xF8) << 2;
			}
			scroll.w = !scroll.w;
			break;

		case 6: // $2006; PPUADDR (write-only)
			if (scroll.w == 0) {
				scroll.t = scroll.t & 0xFF | (data & 0x3F) << 8; // Set bits 13-8 of 't' to bits 5-0 of 'data', and clear bit 14 of 't'
			}
			else {
				scroll.t = scroll.t & 0xFF00 | data; // Set the lower byte of 't' to 'data'
				scroll.v = scroll.t;
				SetA12(scroll.v & 0x1000);
			}
			scroll.w = !scroll.w;
			break;

		case 7: // $2007; PPUDATA (read/write)
			// Outside of rendering, write the value and add either 1 or 32 to v.
			// During rendering, the write is not done, unless it is to palette ram. Else, both coarse x and y are incremented.
			if (InVblank() || !rendering_is_enabled) {
				WriteMemory(scroll.v & 0x3FFF, data); // Only bits 0-13 of v are used; the PPU memory space is 14 bits wide.
				scroll.v += (ppuctrl.incr_mode ? 32 : 1);
				SetA12(scroll.v & 0x1000);
			}
			else if ((scroll.v & 0x3FFF) >= 0x3F00) {
				WritePaletteRAM(scroll.v, data);
				SetA12(scroll.v & 0x1000);
				// Do not increment scroll.v
			}
			else {
				scroll.IncrementCoarseX();
				scroll.IncrementFineY();
			}
			break;

		default:
			std::unreachable();
		}
	}


	void WriteOAMDMA(u8 data)
	{
		open_bus_io.Write(data);
		// Perform an OAM DMA transfer. Writing $XX will upload 256 bytes of data from CPU page $XX00-$XXFF to the internal PPU OAM.
		// It is done by the cpu, so the cpu will be suspended during this time.
		// The writes to OAM will start at the current value of OAMADDR (OAM will be cycled if OAMADDR > 0)
		// TODO: what happens if OAMDMA is written to while a transfer is already taking place?
		CPU::PerformOamDmaTransfer(data, oam.data(), oamaddr);
	}


	u8 OpenBusIO::Read(u8 mask)
	{  
		/* Reading the bits of open bus with the bits determined by 'mask' does not refresh those bits. */
		return value & mask;
	}


	void OpenBusIO::Write(u8 data)
	{  
		/* Writing to any PPU register sets the entire decay register to the value written, and refreshes all bits. */
		UpdateDecayOnIOAccess(0xFF);
		value = data;
	}


	void OpenBusIO::UpdateValue(u8 data, u8 mask)
	{  
		/* Here, the bits of open bus determined by the mask are updated with the supplied data. Also, these bits are refreshed, but not the other ones. */
		UpdateDecayOnIOAccess(mask);
		value = data & mask | value & ~mask;
	}


	void OpenBusIO::UpdateDecayOnIOAccess(u8 mask)
	{
		/* Optimization; a lot of the time, the mask will be $FF. */
		if (mask == 0xFF) {
			ppu_cycles_since_refresh.fill(0);
			decayed.fill(false);
		}
		else {
			/* Refresh the bits given by the mask */
			for (int n = 0; n < 8; n++) {
				if (mask & 1 << n) {
					ppu_cycles_since_refresh[n] = 0;
					decayed[n] = false;
				}
			}
		}
	}


	void OpenBusIO::UpdateDecay(uint elapsed_ppu_cycles)
	{
		/* Each bit of the open bus byte can decay at different points, depending on when a particular bit was read/written to last time. */
		for (int n = 0; n < 8; n++) {
			if (!decayed[n]) {
				ppu_cycles_since_refresh[n] += elapsed_ppu_cycles;
				if (ppu_cycles_since_refresh[n] >= decay_ppu_cycle_length) {
					value &= ~(1 << n);
					decayed[n] = true;
				}
			}
		}
	}


	void ScrollRegisters::IncrementCoarseX()
	{
		if ((v & 0x1F) == 0x1F) { // if coarse x = 31
			v &= ~0x1F; // set course x = 0
			v ^= 0x400; // switch horizontal nametable by toggling bit 10
		}
		else {
			v++; // increment coarse X
		}
	}


	void ScrollRegisters::IncrementFineY()
	{
		if ((v & 0x7000) == 0x7000) { // if fine y == 7
			v &= ~0x7000; // set fine y = 0
			if ((v & 0x3A0) == 0x3A0) { // if course y is 29 or 31
				if ((v & 0x40) == 0) { // if course y is 29
					v ^= 0x800; // switch vertical nametable
				}
				v &= ~0x3E0; // set course y = 0
			}
			else {
				v += 0x20; // increment coarse y
			}
		}
		else {
			v += 0x1000; // increment fine y
		}
	}


	void SpriteEvaluation::Restart() 
	{ 
		num_sprites_copied = sprite_index = byte_index = idle = 0; 
	}
	
	
	void SpriteEvaluation::Reset()
	{
		Restart(); 
		sprite_0_included_current_scanline = sprite_0_included_next_scanline = false;
	}


	void SpriteEvaluation::IncrementSpriteIndex()
	{
		if (++sprite_index == 64) {
			idle = true;
		}
	}


	void SpriteEvaluation::IncrementByteIndex()
	{
		// Check whether we have copied all four bytes of a sprite yet.
		if (++byte_index == 4) {
			// Move to the next sprite in OAM (by incrementing n). 
			if (sprite_index == 0) {
				sprite_0_included_next_scanline = true;
				sprite_index = 1;
			}
			else {
				IncrementSpriteIndex();
			}
			byte_index = 0;
			num_sprites_copied++;
		}
	}


	void TileFetcher::StartOver()
	{
		cycle_step = 0;
	}


	void SetA12(bool new_val)
	{
		if (a12 ^ new_val) {
			if (new_val == 1) {
				if (cpu_cycles_since_a12_set_low >= 3) {
					Cartridge::ClockIRQ();
				}
			}
			else {
				cpu_cycles_since_a12_set_low = 0;
			}
			a12 = new_val;
		}
	}


	u8 ReadPaletteRAM(u16 addr)
	{
		addr &= 0x1F;
		// Addresses $3F10/$3F14/$3F18/$3F1C are mirrors of $3F00/$3F04/$3F08/$3F0C
		// Note: bits 4-0 of all mirrors have the form 1xy00, and the redirected addresses have the form 0xy00
		if ((addr & 0x13) == 0x10) {
			addr -= 0x10;
		}
		// In greyscale mode, use colors only from the grey column: $00, $10, $20, $30.
		if (ppumask.greyscale) {
			return palette_ram[addr & 0x30];
		}
		return palette_ram[addr];
	}


	void WritePaletteRAM(u16 addr, u8 data)
	{
		addr &= 0x1F;
		data &= 0x3F; // Each value is 6 bits (0-63)
		if ((addr & 0x13) == 0x10) {
			addr -= 0x10;
		}
		if (ppumask.greyscale) {
			palette_ram[addr & 0x30] = data;
		}
		palette_ram[addr] = data;
	}


	void CheckNMI()
	{
		/* The PPU pulls /NMI low only if both PPUCTRL.7 and PPUSTATUS.7 are set.
		   Do not call CPU::SetNMILow() if NMI is already low; this would cause multiple interrupts to be handled for the same signal. */
		if (ppuctrl.nmi_enable && ppustatus.vblank) {
			if (nmi_line == 1) {
				CPU::SetNmiLow();
				nmi_line = 0;
			}
		}
		else if (nmi_line == 0) {
			CPU::SetNmiHigh();
			nmi_line = 1;
		}
	}


	void UpdateSpriteEvaluation()
	{
		/* Cycles   1-64: secondary OAM is initialized to $FF. Here: do everything at cycle 65.
		   Cycles 65-256: read oam, evaluate sprites and copy data into secondary oam.
						  Read from oam on odd cycles, copy into secondary oam on even cycles.
						  Here: do both things on even cycles.
			TODO: make more accurate.
		*/
		if (scanline_cycle < 65) {
			return;
		}
		if (scanline_cycle == 65) {
			secondary_oam.fill(0xFF);
			oamaddr_at_cycle_65 = oamaddr;
			sprite_evaluation.Restart();
			return;
		}
		if ((scanline_cycle & 1) || sprite_evaluation.idle) {
			return;
		}
		// Fetch the next entry in OAM
		// The value of OAMADDR as it were at dot 65 is used as an offset to the address here.
		// If OAMADDR is unaligned and does not point to the y-position (first byte) of an OAM entry, then whatever it points to will be reinterpreted as a y position, and the following bytes will be similarly reinterpreted.
		// When the end of OAM is reached, no more sprites will be found (it will not wrap around to the start of OAM).
		auto addr = oamaddr_at_cycle_65 + 4 * sprite_evaluation.sprite_index + sprite_evaluation.byte_index;
		if (addr >= oam.size()) {
			sprite_evaluation.idle = true;
			return;
		}
		u8 oam_entry = oam[addr];
		if (sprite_evaluation.num_sprites_copied < 8) {
			// Copy the read oam entry into secondary oam. Note that this occurs even if this is the first byte of a sprite, and we later decide not to copy the rest of it due to it not being in range!
			secondary_oam[4 * sprite_evaluation.num_sprites_copied + sprite_evaluation.byte_index] = oam_entry;
			if (sprite_evaluation.byte_index == 0) { // Means that the read oam entry is being interpreted as a y-position.
				// If the y-position is in range, copy the three remaining bytes for that sprite. Else move on to the next sprite.
				if (scanline >= oam_entry && scanline < oam_entry + (ppuctrl.sprite_height ? 16 : 8)) {
					sprite_evaluation.byte_index = 1;
				}
				else {
					sprite_evaluation.IncrementSpriteIndex();
				}
			}
			else {
				sprite_evaluation.IncrementByteIndex();
			}
		}
		else {
			if (scanline >= oam_entry && scanline < oam_entry + (ppuctrl.sprite_height ? 16 : 8)) {
				// If a ninth in-range sprite is found, set the sprite overflow flag.
				ppustatus.sprite_overflow = 1;
				// On real hw, the ppu will continue scanning oam after setting this.
				// However, none of it will have an effect on anything other than n and m, which is not visible from the rest of the ppu and system as a whole, so we can start idling from here.
				// Note also that the sprite overflow flag is not writeable by the cpu, and cleared only on the pre-render scanline. Thus, setting it more than one time will not be any different from setting it only once.
				sprite_evaluation.idle = true;
			}
			else {
				// hw bug: increment both n and m (instead of just n)
				sprite_evaluation.IncrementByteIndex();
				sprite_evaluation.IncrementSpriteIndex();
			}
		}
	}


	// Get an actual NES color (indexed 0-63) from a bg or sprite color id (0-3), given the palette id (0-3)
	template<TileType tile_type>
	u8 GetNESColorFromColorID(u8 col_id, u8 palette_id)
	{
		if (rendering_is_enabled) {
			// If the color ID is 0, then the 'universal background color', located at $3F00, is used.
			if (col_id == 0) {
				return ReadPaletteRAM(0);
			}
			// For bg tiles, two consecutive bits of an attribute table byte holds the palette number (0-3). These have already been extracted beforehand (see the updating of the '' variable)
			// For sprites, bits 1-0 of the 'attribute byte' (byte 2 from OAM) give the palette number.
			// Each bg and sprite palette consists of three bytes (describing the actual NES colors for color ID:s 1, 2, 3), starting at $3F01, $3F05, $3F09, $3F0D respectively for bg tiles, and $3F11, $3F15, $3F19, $3F1D for sprites
			auto palette_ram_addr = col_id + 4 * palette_id;
			if constexpr (tile_type == TileType::OBJ) {
				palette_ram_addr += 0x10;
			}
			return ReadPaletteRAM(palette_ram_addr);
		}
		else {
			// If rendering is disabled, show the backdrop colour. 
			// Background palette hack: if the current vram address is in palette "territory", the colour at the current vram address is used, not $3F00.
			if ((scroll.v & 0x7F00) == 0x3F00) {
				return ReadPaletteRAM(scroll.v);
			}
			else {
				return ReadPaletteRAM(0);
			}
		}
	}


	void PushPixelToFramebuffer(const u8 nes_col)
	{
		// From the nes colour (0-63), get an RGB24 colour from the predefined palette
		// The palette from https://wiki.nesdev.org/w/index.php?title=PPU_palettes#2C02 was used for this
		static constexpr std::array<RGB, 64> palette = { {
			{ 84,  84,  84}, {  0,  30, 116}, {  8,  16, 144}, { 48,   0, 136}, { 68,   0, 100}, { 92,   0,  48}, { 84,   4,   0}, { 60,  24,   0},
			{ 32,  42,   0}, {  8,  58,   0}, {  0,  64,   0}, {  0,  60,   0}, {  0,  50,  60}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
			{152, 150, 152}, {  8,  76, 196}, { 48,  50, 236}, { 92,  30, 228}, {136,  20, 176}, {160,  20, 100}, {152,  34,  32}, {120,  60,   0},
			{ 84,  90,   0}, { 40, 114,   0}, {  8, 124,   0}, {  0, 118,  40}, {  0, 102, 120}, {  0,   0,   0}, {  0,   0,   0}, {  0,   0,   0},
			{236, 238, 236}, { 76, 154, 236}, {120, 124, 236}, {176,  98, 236}, {228,  84, 236}, {236,  88, 180}, {236, 106, 100}, {212, 136,  32},
			{160, 170,   0}, {116, 196,   0}, { 76, 208,  32}, { 56, 204, 108}, { 56, 180, 204}, { 60,  60,  60}, {  0,   0,   0}, {  0,   0,   0},
			{236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236}, {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 194, 144},
			{204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180}, {160, 214, 228}, {160, 162, 160}, {  0,   0,   0}, {  0,   0,   0}
		} };

		RGB col = palette[nes_col];
		framebuffer[framebuffer_pos++] = col.r;
		framebuffer[framebuffer_pos++] = col.g;
		framebuffer[framebuffer_pos++] = col.b;
		pixel_x_pos++;
	}


	void ShiftPixel()
	{
		/* TODO: not clear if pixel colours should be 0 if rendering is disabled */

		// Fetch one bit from each of the two bg shift registers containing pattern table data for the current tile, forming the colour id for the current bg pixel.
		// If the PPUMASK_bg_left_col_enable flag is not set, then the background is not rendered in the leftmost 8 pixel columns.
		u8 bg_col_id = [&] {
			if (ppumask.bg_enable && (pixel_x_pos >= 8 || ppumask.bg_left_col_enable)) {
				return ((bg_pattern_shift_reg[0] << scroll.x) & 0x8000) >> 15 | ((bg_pattern_shift_reg[1] << scroll.x) & 0x8000) >> 14;
			}
			else {
				return 0;
			}
		}();
		bg_pattern_shift_reg[0] <<= 1;
		bg_pattern_shift_reg[1] <<= 1;
		// Decrement the x-position counters for all 8 sprites. If a counter is 0, the sprite becomes 'active', and the shift registers for the sprite is shifted once every cycle
		// The current pixel for each 'active' sprite is checked, and the first non-transparent pixel moves on to a multiplexer, where it joins the BG pixel.
		u8 sprite_col_id = 0;
		u8 sprite_index = 0; // (0-7)
		if (ppumask.sprite_enable) {
			bool opaque_pixel_found = false;
			for (int i = 0; i < 8; i++) {
				bool sprite_is_in_range = sprite_x_pos_counter[i] <= 0 && sprite_x_pos_counter[i] > -8;
				if (sprite_is_in_range) {
					// If the PPUMASK_sprite_left_col_enable flag is not set, then sprites are not rendered in the leftmost 8 pixel columns.
					if (!opaque_pixel_found && (pixel_x_pos >= 8 || ppumask.sprite_left_col_enable)) {
						u8 offset = -sprite_x_pos_counter[i]; // Which pixel of the sprite line to render.
						if (sprite_attribute_latch[i] & 0x40) { // flip sprite horizontally 
							offset = 7 - offset;
						}
						u8 col_id = ((sprite_pattern_shift_reg[2 * i] << offset) & 0x80) >> 7 | ((sprite_pattern_shift_reg[2 * i + 1] << offset) & 0x80) >> 6;
						if (col_id != 0) {
							sprite_col_id = col_id;
							sprite_index = i;
							opaque_pixel_found = true;
						}
					}
				}
				sprite_x_pos_counter[i]--;
			}
			// Set the sprite zero hit flag if all conditions below are met. Sprites must be enabled.
			if (!ppustatus.sprite_0_hit && // The flag has not already been set this frame
				sprite_evaluation.sprite_0_included_current_scanline && sprite_index == 0 && // The current sprite is the 0th sprite in OAM
				bg_col_id != 0 && sprite_col_id != 0 && // The bg and sprite colour IDs are not 0, i.e. both pixels are opaque
				ppumask.bg_enable && // Both bg and sprite rendering must be enabled
				(pixel_x_pos >= 8 || (ppumask.bg_left_col_enable && ppumask.sprite_left_col_enable)) && // If the pixel-x-pos is between 0 and 7, the left-side clipping window must be disabled for both bg tiles and sprites.
				pixel_x_pos != 255)                                                                     // The pixel-x-pos must not be 255
			{
				// Due to how internal rendering works, the sprite 0 hit flag will be set at the third tick of a scanline at the earliest.
				if (scanline_cycle >= 2) {
					ppustatus.sprite_0_hit = 1;
				}
				else {
					set_sprite_0_hit_flag = true;
				}
			}
		}
		else {
			for (auto& x_pos : sprite_x_pos_counter) {
				--x_pos;
			}
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
		bool sprite_priority = sprite_attribute_latch[sprite_index] & 0x20;
		u8 col = [&] {
			if (sprite_col_id > 0 && (sprite_priority == 0 || bg_col_id == 0)) {
				return GetNESColorFromColorID<TileType::OBJ>(sprite_col_id, sprite_attribute_latch[sprite_index] & 3);
			}
			// Fetch one bit from each of the two bg shift registers containing the palette id for the current tile.
			u8 bg_palette_id = ((bg_palette_attr_reg[0] << scroll.x) & 0x8000) >> 15 | ((bg_palette_attr_reg[1] << scroll.x) & 0x8000) >> 14;
			return GetNESColorFromColorID<TileType::BG>(bg_col_id, bg_palette_id);
		}();
		bg_palette_attr_reg[0] <<= 1;
		bg_palette_attr_reg[1] <<= 1;

		PushPixelToFramebuffer(col);
	}


	void ReloadBackgroundShiftRegisters()
	{
		// Reload the lower 8 bits of the two 16-bit background shifters with pattern data for the next tile.
		// The lower byte is already 0x00.
		bg_pattern_shift_reg[0] |= tile_fetcher.pattern_table_tile_low;
		bg_pattern_shift_reg[1] |= tile_fetcher.pattern_table_tile_high;
		// For bg tiles, an attribute table byte holds palette info. Each table entry controls a 32x32 pixel metatile.
		// The byte is divided into four 2-bit areas, which each control a 16x16 pixel metatile
		// Denoting the four 16x16 pixel metatiles by 'bottomright', 'bottomleft' etc, then: value = (bottomright << 6) | (bottomleft << 4) | (topright << 2) | (topleft << 0)
		// We find which quadrant our 8x8 tile lies in. Then, the two extracted bits give the palette number (0-3) used for the tile
		u8 palette_id = tile_fetcher.attribute_table_byte >> (2 * tile_fetcher.attribute_table_quadrant) & 3;
		// The LSB of the attribute registers are filled with the palette id (0-3) (next tile to be rendered after the current one).
		// Note: the same palette id is used for an entire tile, so the LSB is either set to $00 or $FF
		bg_palette_attr_reg[0] |= 0xFF * bool(palette_id & 0x01);
		bg_palette_attr_reg[1] |= 0xFF * bool(palette_id & 0x02);
	}


	void ReloadSpriteShiftRegisters(uint sprite_index)
	{
		// Reload the two 8-bit sprite shift registers (of index 'sprite_index') with pattern data for the next tile.
		// If 'sprite_index' is not less than the number of sprites copied from OAM, the registers are loaded with transparent data instead.
		if (sprite_index < sprite_evaluation.num_sprites_copied) {
			sprite_pattern_shift_reg[2 * sprite_index    ] = tile_fetcher.pattern_table_tile_low;
			sprite_pattern_shift_reg[2 * sprite_index + 1] = tile_fetcher.pattern_table_tile_high;
		}
		else {
			sprite_pattern_shift_reg[2 * sprite_index    ] = 0;
			sprite_pattern_shift_reg[2 * sprite_index + 1] = 0;
		}
	}


	void UpdateBGTileFetching()
	{
		/* Each memory access is two cycles long. On the first one, the address is loaded.
		   On the second one, the read/write is made. https://www.nesdev.org/2C02%20technical%20reference.TXT */
		switch (tile_fetcher.cycle_step++) {
		case 0: /* Compose address for nametable byte. */
			/* Composition of the nametable address:
			  10 NN YYYYY XXXXX
			  || || ||||| +++++-- Coarse X scroll
			  || || +++++-------- Coarse Y scroll
			  || ++-------------- Nametable select
			  ++----------------- Nametable base address ($2000)
			*/
			tile_fetcher.addr = 0x2000 | scroll.v & 0xFFF;
			SetA12(0);
			break;

		case 1: /* Fetch nametable byte. */
			tile_fetcher.tile_num = Cartridge::ReadNametableRAM(tile_fetcher.addr);
			break;

		case 2: /* Compose address for attribute table byte. */
			/* Composition of the attribute address:
			  10 NN 1111 YYY XXX
			  || || |||| ||| +++-- High 3 bits of coarse X scroll (x/4)
			  || || |||| +++------ High 3 bits of coarse Y scroll (y/4)
			  || || ++++---------- Attribute offset (960 = $3c0 bytes)
			  || ++--------------- Nametable select
			  ++------------------ Nametable base address ($2000)
			*/
			tile_fetcher.addr = 0x23C0 | (scroll.v & 0x0C00) | ((scroll.v >> 4) & 0x38) | ((scroll.v >> 2) & 0x07);
			SetA12(0);
			// Determine in which quadrant (0-3) of the 32x32 pixel metatile that the current tile is in
			// topleft == 0, topright == 1, bottomleft == 2, bottomright = 3
			// scroll-x % 4 and scroll-y % 4 give the "tile-coordinates" of the current tile in the metatile
			tile_fetcher.attribute_table_quadrant = 2 * ((scroll.v & 0x60) > 0x20) + ((scroll.v & 0x03) > 0x01);
			break;

		case 3: /* Fetch atttribute table byte. */
			tile_fetcher.attribute_table_byte = Cartridge::ReadNametableRAM(tile_fetcher.addr);
			break;

		case 4: { /* Compose address for pattern table tile low. */
			/* Composition of the pattern table address for BG tiles:
			  H RRRR CCCC P yyy
			  | |||| |||| | +++-- The row number within a tile: fine Y scroll
			  | |||| |||| +------ Bit plane (0: "lower"; 1: "upper")
			  | |||| ++++-------- Tile column
			  | ++++------------- Tile row
			  +------------------ Half of pattern table (0: "left"; 1: "right"); dependent on PPUCTRL flags
			  RRRR CCCC == the nametable byte fetched in step 1
			*/
			u16 pattern_table_half = ppuctrl.bg_tile_select ? 0x1000 : 0x0000;
			tile_fetcher.addr = pattern_table_half | tile_fetcher.tile_num << 4 | scroll.v >> 12;
			SetA12(pattern_table_half);
			break;
		}

		case 5: /* Fetch pattern table tile low. */
			tile_fetcher.pattern_table_tile_low = Cartridge::ReadCHR(tile_fetcher.addr);
			break;

		case 6: /* Compose address for pattern table tile high. This could be done in step 7 instead; it does not affect A12. */
			// Technically, a game could change PPUCTRL_BG_TILE_SELECT here (?). What game would do that?
			tile_fetcher.addr |= 0x0008;
			SetA12(tile_fetcher.addr & 0x1000);
			break;

		case 7: /* Fetch pattern table tile high. */
			tile_fetcher.pattern_table_tile_high = Cartridge::ReadCHR(tile_fetcher.addr);
			// Increment coarse x after fetching the tile.
			scroll.IncrementCoarseX();
			// Increment the coarse Y scroll at cycle 256, after all BG tiles have been fetched (will be the case when 'cycle_step' is 7)
			if (scanline_cycle == 256) {
				scroll.IncrementFineY();
			}
			break;

		default:
			std::unreachable();
		}
	}


	void UpdateSpriteTileFetching()
	{
		switch (tile_fetcher.cycle_step++) {
		case 0: case 2: /* Prepare address for garbage nametable fetches. The important thing is to update A12. */
			SetA12(ppuctrl.bg_tile_select); // TODO: should PPUCTRL_SPRITE_TILE_SELECT be used instead? Probably not.
			break;

		case 1: case 3: /* Garbage nametable fetches. */
			break;

		case 4: /* Compose address for pattern table tile low. */
		{
			/* Composition of the pattern table address for 8x8 sprites:
			  H RRRR CCCC P yyy
			  | |||| |||| | +++-- The row number within a tile: sprite_y_pos - fine_y_scroll
			  | |||| |||| +------ Bit plane (0: "lower"; 1: "upper")
			  | |||| ++++-------- Tile column
			  | ++++------------- Tile row
			  +------------------ Half of pattern table (0: "left"; 1: "right"); dependent on PPUCTRL flags
			  RRRR CCCC == the sprite tile index number fetched from secondary OAM during cycles 257-320

			Composition of the pattern table adress for 8x16 sprites:
			  H RRRR CCC S P yyy
			  | |||| ||| | | +++-- The row number within a tile: sprite_y_pos - fine_y_scroll. TODO probably not correct
			  | |||| ||| | +------ Bit plane (0: "lower"; 1: "upper")
			  | |||| ||| +-------- Sprite tile half (0: "top"; 1: "bottom")
			  | |||| +++---------- Tile column
			  | ++++-------------- Tile row
			  +------------------- Half of pattern table (0: "left"; 1: "right"); equal to bit 0 of the sprite tile index number fetched from secondary OAM during cycles 257-320
			  RRRR CCC == upper 7 bits of the sprite tile index number fetched from secondary OAM during cycles 257-320
			*/
			// TODO: not sure if scroll.v should be used instead of current_scanline
			auto scanline_sprite_y_delta = scanline - tile_fetcher.sprite_y_pos; // delta between scanline and sprite position (0-15)
			bool flip_sprite_y = tile_fetcher.sprite_attr & 0x80;
			auto sprite_row_num = scanline_sprite_y_delta & 7; // which row of the tile the scanline falls on
			if (flip_sprite_y) {
				sprite_row_num = 7 - sprite_row_num;
			}
			if (ppuctrl.sprite_height) { // 8x16 sprites
				bool sprite_table_half = tile_fetcher.tile_num & 0x01;
				u8 tile_num = tile_fetcher.tile_num & 0xFE; // Tile number of the top of sprite (0 to 254; bottom half gets the next tile)
				// Check if we are on the top or bottom tile of the sprite.
				// If sprites are flipped vertically, the top and bottom tiles are flipped.
				bool on_bottom_tile = scanline_sprite_y_delta > 7;
				bool fetch_bottom_tile = on_bottom_tile ^ flip_sprite_y;
				tile_num += fetch_bottom_tile;
				tile_fetcher.addr = sprite_table_half << 12 | tile_num << 4 | sprite_row_num;
			}
			else { // 8x8 sprites
				tile_fetcher.addr = (ppuctrl.sprite_tile_select ? 0x1000 : 0x0000) | tile_fetcher.tile_num << 4 | sprite_row_num;
			}
			SetA12(tile_fetcher.addr & 0x1000);
			break;
		}

		case 5: /* Fetch pattern table tile low. */
			tile_fetcher.pattern_table_tile_low = Cartridge::ReadCHR(tile_fetcher.addr);
			break;

		case 6: /* Compose address for pattern table tile high. This could be done in step 7 instead. */
			tile_fetcher.addr |= 0x0008;
			SetA12(tile_fetcher.addr & 0x1000);
			break;

		case 7: /* Fetch pattern table tile high. */
			tile_fetcher.pattern_table_tile_high = Cartridge::ReadCHR(tile_fetcher.addr);
			break;

		default: // impossible
			std::unreachable();
		}
	}


	// Reading and writing done internally by the ppu
	u8 ReadMemory(const u16 addr)
	{
		// $0000-$1FFF - Pattern tables; maps to CHR ROM/RAM on the game cartridge
		if (addr <= 0x1FFF) {
			return Cartridge::ReadCHR(addr);
		}
		// $3F00-$3F1F - Palette RAM indeces. 
		// $3F20-$3FFF - mirrors of $3F00-$3F1F
		else if ((addr & 0x3F00) == 0x3F00) {
			return ReadPaletteRAM(addr);
		}
		// $2000-$2FFF - Nametables; internal ppu vram.
		// $3000-$3EFF - mirror of $2000-$2EFF
		else { 
			return Cartridge::ReadNametableRAM(addr);
		}
	}


	void WriteMemory(const u16 addr, const u8 data)
	{
		// $0000-$1FFF - Pattern tables; maps to CHR ROM/RAM on the game cartridge
		if (addr <= 0x1FFF) {
			Cartridge::WriteCHR(addr, data);
		}
		// $3F00-$3F1F - Palette RAM indeces. 
		// $3F20-$3FFF - mirrors of $3F00-$3F1F
		else if ((addr & 0x3F00) == 0x3F00) {
			WritePaletteRAM(addr, data);
		}
		// $2000-$2FFF - Nametables; internal ppu vram.
		// $3000-$3EFF - mirror of $2000-$2EFF
		else {
			Cartridge::WriteNametableRAM(addr, data);
		}
	}


	void PrepareForNewFrame()
	{
		odd_frame = !odd_frame;
		framebuffer_pos = 0;
	}


	void PrepareForNewScanline()
	{
		if (scanline == System::standard.num_scanlines - 2) { // E.g. on NTSC, num_scanlines == 262, and we jump straight from 260 to -1 (pre-render).
			scanline = pre_render_scanline;
			PrepareForNewFrame();
		}
		else {
			scanline++;
		}
		pixel_x_pos = 0;
		sprite_evaluation.sprite_0_included_current_scanline = sprite_evaluation.sprite_0_included_next_scanline;
		sprite_evaluation.sprite_0_included_next_scanline = false;
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(open_bus_io);
		stream.StreamPrimitive(scroll);
		stream.StreamPrimitive(sprite_evaluation);
		stream.StreamPrimitive(tile_fetcher);

		stream.StreamPrimitive(a12);
		stream.StreamPrimitive(cpu_cycles_since_a12_set_low);

		stream.StreamPrimitive(cycle_340_was_skipped_on_last_scanline);
		stream.StreamPrimitive(nmi_line);
		stream.StreamPrimitive(odd_frame);
		stream.StreamPrimitive(rendering_is_enabled);
		stream.StreamPrimitive(set_sprite_0_hit_flag);

		stream.StreamPrimitive(pixel_x_pos);
		stream.StreamPrimitive(ppuctrl);
		stream.StreamPrimitive(ppumask);
		stream.StreamPrimitive(ppustatus);
		stream.StreamPrimitive(ppuscroll);
		stream.StreamPrimitive(ppudata);
		stream.StreamPrimitive(oamaddr);
		stream.StreamPrimitive(oamaddr_at_cycle_65);
		stream.StreamPrimitive(oamdma);

		stream.StreamPrimitive(scanline);

		stream.StreamPrimitive(cpu_cycle_counter);
		stream.StreamPrimitive(framebuffer_pos);
		stream.StreamPrimitive(scanline_cycle);
		stream.StreamPrimitive(secondary_oam_sprite_index);

		stream.StreamArray(oam);
		stream.StreamArray(palette_ram);
		stream.StreamArray(secondary_oam);

		stream.StreamArray(sprite_attribute_latch);
		stream.StreamArray(sprite_pattern_shift_reg);
		stream.StreamArray(bg_palette_attr_reg);
		stream.StreamArray(bg_pattern_shift_reg);

		stream.StreamArray(sprite_x_pos_counter);

		stream.StreamVector(framebuffer);
	}
}