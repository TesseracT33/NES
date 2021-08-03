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
	Cartridge::Mapper mapper = Cartridge::GetMapperFromCartridge(rom_path);
	if (mapper == Cartridge::Mapper::INVALID)
		return;
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