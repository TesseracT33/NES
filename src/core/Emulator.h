#pragma once

#include <chrono>
#include <vector>

#include "APU.h"
#include "Bus.h"
#include "Cartridge.h"
#include "Component.h"
#include "CPU.h"
#include "Joypad.h"
#include "PPU.h"

class Emulator final : public Serializable
{
public:
	Emulator();

	
	void StartGame(const char* rom_path);
	void MainLoop();

	void Pause();
	void Reset();
	void Resume();
	void Stop();

	void LoadState();
	void SaveState();

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

private:
	const unsigned cycles_per_sec_NTSC = 1789773;
	const unsigned cycles_per_sec_PAL = 1662607;
	const unsigned cycles_per_sec_Dendy = 1773448;

	const unsigned cycles_per_frame_NTSC = 0;

	const unsigned microseconds_per_frame_NTSC = 0;

	bool emu_is_paused = false, emu_is_running = false;
	bool emu_speed_uncapped = false;
	bool load_state_on_next_cycle = false, save_state_on_next_cycle = false;

	unsigned cycle_counter;

	APU apu;
	Bus bus;
	Cartridge cartridge;
	CPU cpu;
	Joypad joypad;
	PPU ppu;

	std::vector<Component*> components;

	char* current_rom_path;

	void AddComponents();
	void ConnectSystemComponents();
};

