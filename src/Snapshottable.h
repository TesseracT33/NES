#pragma once

#include "SerializationStream.h"

class Snapshottable
{
public:
	virtual void StreamState(SerializationStream& stream) = 0;
};