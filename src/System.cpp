module System;

import APU;
import PPU;

namespace System
{
	void StepAllComponentsButCpu()
	{
		APU::Update();
		PPU::Update();
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(standard);
	}
}