#pragma once

#include "../Configurable.h"
#include "../Snapshottable.h"

#include "NES.h"

class Component : public Snapshottable, public Configurable
{
public:
	Component() = default;
	Component(NES* nes) : nes(nes) {};

	virtual void State(Serialization::Functor& functor) override {};
	virtual void Configure(Serialization::Functor& functor) override {};
	virtual void SetDefaultConfig() override {};

	void AttachNES(NES* nes) { this->nes = nes; };

protected:
	NES* nes;
};