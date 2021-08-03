#pragma once

#include <vector>

#include "Cartridge.h"
#include "Component.h"

class Emulator
{
public:
	virtual void StartGame(const char* rom_path) = 0;
	virtual void MainLoop();

	void Pause();
	void Reset();
	void Resume();
	void Stop();

	void LoadState();
	void SaveState();

private:
	virtual void AddComponents() = 0;
	std::vector<Component*> components;

	Cartridge* cartridge;
};

