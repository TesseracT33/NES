#pragma once

#include "Component.h"

class PPU final : public Component
{
private:
	static const unsigned resolution_y = 240;

	struct Memory
	{
		u8 vram[0x1000]{};
		u8 palette_ram[0x20]{};
	} memory;

	bool NMI_occured, NMI_output;
};

