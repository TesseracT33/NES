#pragma once

#include "Component.h"

class APU final : public Component
{
public:
	void Reset();
	void Update();

private:
	unsigned LFSR : 15;
};

