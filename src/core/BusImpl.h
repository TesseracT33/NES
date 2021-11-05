#pragma once

#include <array>

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
	CPU* cpu;
	Joypad* joypad;
	PPU* ppu;

	std::shared_ptr<BaseMapper> mapper; /* This is heap-allocated, the other components are not. */

	void Initialize();
	void Reset();

	/* CPU reads and writes */
	u8 Read(u16 addr) override;
	void Write(u16 addr, u8 data) override;

	// Reads and writes, but also advances the state machine by one cycle
	u8 ReadCycle(u16 addr) override;
	void WriteCycle(u16 addr, u8 data) override;

	// Simply advance the state machine
	void WaitCycle() override;

	void State(Serialization::BaseFunctor& functor) override;

private:
	std::array<u8, 0x800> ram{}; /* $0000-$07FF, repeated three times until $1FFF */
	std::array<u8, 0x08> apu_io_test{}; /* $4018-$401F */

	void UpdateLogging();
};

