#include "Emulator.h"


void Emulator::LoadState()
{
	std::ifstream ifs{ "state.bin" };
	for (Component* component : components)
		component->Deserialize(ifs);
}


void Emulator::SaveState()
{
	std::ofstream ofs{ "state.bin", std::ofstream::out };
	for (Component* component : components)
		component->Serialize(ofs);
}


void Emulator::StartGame(const char* rom_path)
{
	bool rom_loaded = cartridge.ReadRomFile(rom_path);
	if (!rom_loaded) return;

	MainLoop();
}


void Emulator::MainLoop()
{

}


void Emulator::Pause()
{

}


void Emulator::Reset()
{

}


void Emulator::Resume()
{

}


void Emulator::Stop()
{

}


void Emulator::AddComponents()
{

}