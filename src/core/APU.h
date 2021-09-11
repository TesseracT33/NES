#pragma once

#include "Bus.h"
#include "Component.h"

class APU final : public Component
{
public:
	void Reset();
	void Update();

	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

private:
	unsigned LFSR : 15;
};

