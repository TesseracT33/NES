#pragma once

#include "Serialization.h"

class Snapshottable
{
public:
	virtual void State(Serialization::Functor& functor) = 0;
};