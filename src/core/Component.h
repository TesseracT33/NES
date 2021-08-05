#pragma once

#include "../Configurable.h"
#include "../Serializable.h"

class Component : public Configurable, public Serializable
{
public:
	virtual void Initialize() {};
	virtual void Reset() {};
	virtual void Update() {};

	virtual void Serialize(std::ofstream& ofs) {};
	virtual void Deserialize(std::ifstream& ifs) {};

	virtual void SaveConfig(std::ofstream& ofs) {};
	virtual void LoadConfig(std::ifstream& ifs) {};
	virtual void SetDefaultConfig() {};
};

