#pragma once

#pragma once
class Observer
{
public:
	virtual void UpdateFPSLabel() = 0;
	unsigned frames_since_update = 0;
};