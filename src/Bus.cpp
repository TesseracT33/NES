module Bus;

import APU;
import Cartridge;
import Debug;
import Joypad;
import PPU;

namespace Bus
{
	constexpr std::string_view IoAddrToString(u16 addr)
	{
		switch (addr) {
		case PPUCTRL: return "PPUCTRL";
		case PPUMASK: return "PPUMASK";
		case PPUSTATUS: return "PPUSTATUS";
		case OAMADDR: return "OAMADDR";
		case OAMDATA: return "OAMDATA";
		case PPUSCROLL: return "PPUSCROLL";
		case PPUADDR: return "PPUADDR";
		case PPUDATA: return "PPUDATA";
		case OAMDMA: return "OAMDMA";
		case SQ1_VOL: return "SQ1_VOL";
		case SQ1_SWEEP: return "SQ1_SWEEP";
		case SQ1_LO: return "SQ1_LO";
		case SQ1_HI: return "SQ1_HI";
		case SQ2_VOL: return "SQ2_VOL";
		case SQ2_SWEEP: return "SQ2_SWEEP";
		case SQ2_LO: return "SQ2_LO";
		case SQ2_HI: return "SQ2_HI";
		case TRI_LINEAR: return "TRI_LINEAR";
		case TRI_LO: return "TRI_LO";
		case TRI_HI: return "TRI_HI";
		case NOISE_VOL: return "NOISE_VOL";
		case NOISE_LO: return "NOISE_LO";
		case NOISE_HI: return "NOISE_HI";
		case DMC_FREQ: return "DMC_FREQ";
		case DMC_RAW: return "DMC_RAW";
		case DMC_START: return "DMC_START";
		case DMC_LEN: return "DMC_LEN";
		case APU_STAT: return "APU_STAT";
		case JOY1: return "JOY1";
		case JOY2: return "JOY2/FRAME_CNT";
		case NMI_VEC: return "NMI_VEC";
		case RESET_VEC: return "RESET_VEC";
		case IRQ_BRK_VEC: return "IRQ_BRK_VEC";
		default: return {};
		}
	}


	u8 Peek(u16 addr)
	{
		// Internal RAM ($0000 - $1FFF)
		if (addr <= 0x1FFF) {
			return ram[addr & 0x7FF]; // wrap address to between 0-0x7FF
		}
		// Cartridge Space ($4020 - $FFFF)
		else if (addr >= 0x4020) {
			return Cartridge::ReadPRG(addr);
		}
		// PPU Registers ($2000 - $3FFF) 
		else if (addr <= 0x3FFF) {
			// Wrap address to between 0x2000-0x2007
			return PPU::PeekRegister(addr & 0x2007);
		}
		// APU & I/O Registers ($4000-$4017)
		else if (addr <= 0x4017) {
			switch (addr) {
			case Addr::OAMDMA: // $4014
				return PPU::PeekOAMDMA();
			case Addr::JOY1: // $4016
			case Addr::JOY2: // $4017
				return Joypad::PeekRegister(addr);
			default:
				return APU::PeekRegister(addr);
			}
		}
		// APU Test Registers ($4018 - $401F)
		else {
			return apu_io_test[addr - 0x4018];
		}
	}


	void PowerOn()
	{
		apu_io_test.fill(0);
		ram.fill(0);
	}


	u8 Read(u16 addr)
	{
		// Internal RAM ($0000 - $1FFF)
		if (addr <= 0x1FFF) {
			return ram[addr & 0x7FF]; // wrap address to between 0-0x7FF
		}
		// Cartridge Space ($4020 - $FFFF)
		else if (addr >= 0x4020) {
			return Cartridge::ReadPRG(addr);
		}
		// PPU Registers ($2000 - $3FFF) 
		else if (addr <= 0x3FFF) {
			// Wrap address to between 0x2000-0x2007
			u8 value = PPU::ReadRegister(addr & 0x2007);
			if constexpr (Debug::log_io) {
				Debug::LogIoRead(addr & 0x2007, value);
			}
			return value;
		}
		// APU & I/O Registers ($4000-$4017)
		else if (addr <= 0x4017) {
			u8 value = [&] {
				switch (addr) {
				case Addr::OAMDMA: // $4014
					return PPU::ReadOAMDMA();
				case Addr::JOY1: // $4016
				case Addr::JOY2: // $4017
					return Joypad::ReadRegister(addr);
				default:
					return APU::ReadRegister(addr);
				}
			}();
			if constexpr (Debug::log_io) {
				Debug::LogIoRead(addr, value);
			}
			return value;
		}
		// APU Test Registers ($4018 - $401F)
		else {
			return apu_io_test[addr - 0x4018];
		}
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamArray(apu_io_test);
		stream.StreamArray(ram);
	}


	void Write(u16 addr, u8 data)
	{
		// Internal RAM ($0000 - $1FFF)
		if (addr <= 0x1FFF) {
			ram[addr & 0x7FF] = data; // wrap address to between 0-0x7FF
		}
		// Cartridge Space ($4020 - $FFFF)
		else if (addr >= 0x4020) {
			Cartridge::WritePRG(addr, data);
		}
		// PPU Registers ($2000 - $3FFF)
		else if (addr <= 0x3FFF) {
			// wrap address to between 0x2000-0x2007 
			PPU::WriteRegister(addr & 0x2007, data);
			if constexpr (Debug::log_io) {
				Debug::LogIoWrite(addr, data);
			}
		}
		// APU & I/O Registers ($4000-$4017)
		else if (addr <= 0x4017) {
			switch (addr) {
			case Addr::OAMDMA: // $4014
				PPU::WriteOAMDMA(data);
				break;
			case Addr::JOY1: // $4016
				Joypad::WriteRegister(addr, data);
				break;
			/* When writing, $4017 accesses FRAME_CNT, not JOY2 */
			default:
				APU::WriteRegister(addr, data);
				break;
			}
			if constexpr (Debug::log_io) {
				Debug::LogIoWrite(addr, data);
			}
		}
		// APU Test Registers ($4018 - $401F)
		else {
			apu_io_test[addr - 0x4018] = data;
		}
	}
}