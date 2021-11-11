#pragma once

#include "SerializationStream.h"

class Configurable
{
public:
	virtual void StreamConfig(SerializationStream& stream) = 0;
	virtual void SetDefaultConfig() = 0;
};