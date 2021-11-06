#pragma once

#include <fstream>

#include "Serialization.h"

class Configurable
{
public:
	virtual void Configure(Serialization::Functor& functor) = 0;
	virtual void SetDefaultConfig() = 0;
};