#pragma once

#include <chrono>
#include <vector>

#include "SDL.h"
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/stdpaths.h>

#include "../Observer.h"
#include "../Snapshottable.h"

#include "APU.h"
#include "BusImpl.h"
#include "Cartridge.h"
#include "Component.h"
#include "CPU.h"
#include "Joypad.h"
#include "PPU.h"

class Emulator final : public Snapshottable, public Configurable
{
public:
	Emulator();

	bool emu_is_paused = false, emu_is_running = false;
	bool emulation_speed_uncapped = false;
	unsigned emulation_speed = 100;

	void StartGame(std::string rom_path);
	void MainLoop();

	void Pause();
	void Reset();
	void Resume();
	void Stop();
	void LoadState();
	void SaveState();

	void State(Serialization::BaseFunctor& functor) override;
	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

	void AddObserver(Observer* observer);
	bool SetupSDLVideo(const void* window_handle);

	void SetEmulationSpeed(unsigned speed);
	void SetWindowScale(unsigned scale) { ppu.SetWindowScale(scale); }
	void SetWindowSize(wxSize size) { ppu.SetWindowSize(size); }

	unsigned GetWindowScale() { return ppu.GetWindowScale(); }
	wxSize GetWindowSize() { return ppu.GetWindowSize(); }

	APU apu;
	BusImpl bus;
	Cartridge cartridge;
	CPU cpu;
	Joypad joypad;
	PPU ppu;

	Observer* gui;

private:
	const unsigned cycles_per_sec_NTSC = 1789773;
	const unsigned cycles_per_sec_PAL = 1662607;
	const unsigned cycles_per_sec_Dendy = 1773448;

	const unsigned cycles_per_frame_NTSC = 0;
	const unsigned cycles_per_frame_PAL = 0;
	const unsigned cycles_per_frame_Dendy = 0;

	const unsigned microseconds_per_frame_NTSC = 0;
	bool load_state_on_next_cycle = false, save_state_on_next_cycle = false;

	unsigned cycle_counter;

	std::vector<Component*> components;

	std::string current_rom_path;
	std::string save_state_path = wxFileName(wxStandardPaths::Get().GetExecutablePath())
		.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + "state.bin";

	std::vector<Snapshottable*> snapshottable_components{};

	void AddComponents();
	//void BuildComponentVector();
	void ConnectSystemComponents();
};

