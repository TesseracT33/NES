#pragma once

#include "../Types.h"
#include "../Utils.h"

#include "Component.h"

class Bus final : public Component
{
public:
	enum Addr : u16
	{
		DMA = 0x4014,
		INPUT_1 = 0x4016,
		INPUT_2 = 0x4017,
	};

	void Initialize() override;
	void Reset() override;

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

	u8 Read(u16 addr);
	void Write(u16 addr, u8 data);

private:
	struct Memory
	{
		u8 ram[0x800];
		u8 ppu_regs[0x8];
		u8 apu_io_regs[0x18];
	} memory;
};

