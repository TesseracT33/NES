#pragma once

#include "../Types.h"

#include "../debug/DebugOptions.h"

class Bus
{
public:
	enum Addr : u16
	{
		// PPU regs
		PPUCTRL   = 0x2000,
		PPUMASK           ,
		PPUSTATUS         ,
		OAMADDR           ,
		OAMDATA           ,
		PPUSCROLL         ,
		PPUADDR           ,
		PPUDATA           ,
		OAMDMA    = 0x4014,

		// CPU regs
		SQ1_VOL    = 0x4000,
		SQ1_SWEEP  = 0x4001,
		SQ1_LO     = 0x4002,
		SQ1_HI     = 0x4003,
		SQ2_VOL    = 0x4004,
		SQ2_SWEEP  = 0x4005,
		SQ2_LO     = 0x4006,
		SQ2_HI     = 0x4007,
		TRI_LINEAR = 0x4008,
		TRI_LO     = 0x400A,
		TRI_HI     = 0x400B,
		NOISE_VOL  = 0x400C,
		NOISE_LO   = 0x400E,
		NOISE_HI   = 0x400F,
		DMC_FREQ   = 0x4010,
		DMC_RAW    = 0x4011,
		DMC_START  = 0x4012,
		DMC_LEN    = 0x4013,
		APU_STAT   = 0x4015,
		JOY1       = 0x4016,
		JOY2       = 0x4017,
		FRAME_CNT  = 0x4017,

		// interrupt vectors
		NMI_VEC     = 0xFFFA,
		RESET_VEC   = 0xFFFC,
		IRQ_BRK_VEC = 0xFFFE
	};

	/* CPU reads/writes/waits, that also advances the state machine by one cycle */
	virtual u8   ReadCycle(u16 addr)           = 0;
	virtual void WaitCycle()                   = 0;
	virtual void WriteCycle(u16 addr, u8 data) = 0;

#ifdef DEBUG
	bool update_logging_on_next_cycle = false;
#endif
};

