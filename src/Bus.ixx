export module Bus;

import NumericalTypes;
import SerializationStream;

import <array>;
import <string_view>;

namespace Bus
{
	export
	{
		enum Addr : u16 {
			// PPU regs
			PPUCTRL = 0x2000,
			PPUMASK,
			PPUSTATUS,
			OAMADDR,
			OAMDATA,
			PPUSCROLL,
			PPUADDR,
			PPUDATA,
			OAMDMA = 0x4014,

			// APU/CPU regs
			SQ1_VOL = 0x4000,
			SQ1_SWEEP = 0x4001,
			SQ1_LO = 0x4002,
			SQ1_HI = 0x4003,
			SQ2_VOL = 0x4004,
			SQ2_SWEEP = 0x4005,
			SQ2_LO = 0x4006,
			SQ2_HI = 0x4007,
			TRI_LINEAR = 0x4008,
			TRI_LO = 0x400A,
			TRI_HI = 0x400B,
			NOISE_VOL = 0x400C,
			NOISE_LO = 0x400E,
			NOISE_HI = 0x400F,
			DMC_FREQ = 0x4010,
			DMC_RAW = 0x4011,
			DMC_START = 0x4012,
			DMC_LEN = 0x4013,
			APU_STAT = 0x4015,
			JOY1 = 0x4016,
			JOY2 = 0x4017,
			FRAME_CNT = 0x4017,

			// interrupt vectors
			NMI_VEC = 0xFFFA,
			RESET_VEC = 0xFFFC,
			IRQ_BRK_VEC = 0xFFFE
		};

		constexpr std::string_view IoAddrToString(u16 addr);
		u8 Read(u16 addr);
		u8 Peek(u16 addr);
		void PowerOn();
		void StreamState(SerializationStream& stream);
		void Write(u16 addr, u8 data);
	}

	std::array<u8, 0x800> ram{}; /* $0000-$07FF, mirrored until $1FFF */
	std::array<u8, 0x08> apu_io_test{}; /* $4018-$401F */
}