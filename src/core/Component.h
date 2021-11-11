#pragma once

#include "../Configurable.h"
#include "../Snapshottable.h"

#include "NES.h"

class Component : public Snapshottable, public Configurable
{
public:
	Component() = default;
	Component(NES* nes) : nes(nes) {};

	virtual void StreamState(SerializationStream& stream) override {};
	virtual void StreamConfig(SerializationStream& stream) override {};
	virtual void SetDefaultConfig() override {};

	void AttachNES(NES* nes) { this->nes = nes; };

protected:
	NES* nes;
};