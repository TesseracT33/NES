#pragma once

#include <vector>

#include "Cartridge.h"
#include "Component.h"

class Emulator
{
public:
	void StartGame(const char* rom_path);
	void MainLoop();

	void Pause();
	void Reset();
	void Resume();
	void Stop();

	void LoadState();
	void SaveState();

private:
	const unsigned cycles_per_sec_NTSC = 1789773;
	const unsigned cycles_per_sec_PAL = 1662607;
	const unsigned cycles_per_sec_Dendy = 1773448;

	virtual void AddComponents() = 0;
	std::vector<Component*> components;

	Cartridge cartridge;
};

