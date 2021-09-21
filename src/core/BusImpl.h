#pragma once

#include "../debug/Logging.h"

#include "APU.h"
#include "Bus.h"
#include "Component.h"
#include "CPU.h"
#include "Joypad.h"
#include "PPU.h"

#include "mappers/BaseMapper.h"

class BusImpl final : public Bus, public Component
{
public:
	APU* apu;
	BaseMapper* mapper;
	CPU* cpu;
	Joypad* joypad;
	PPU* ppu;

	void Initialize();
	void Reset();

	u8 Read(u16 addr) override;
	void Write(u16 addr, u8 data) override;

	// Reads and writes, but also advances the state machine by one cycle
	u8 ReadCycle(u16 addr) override;
	void WriteCycle(u16 addr, u8 data) override;

	// Simply advance the state machine
	void WaitCycle() override;

	void State(Serialization::BaseFunctor& functor) override;

private:
	struct Memory
	{
		u8 ram[0x800];        // $0000-$07FF, repeated three times until $1FFF
		u8 apu_io_test[0x08]; // $4018-$401F
	} memory;

	void UpdateLogging();
};

