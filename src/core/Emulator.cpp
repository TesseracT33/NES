#include "Emulator.h"


Emulator::Emulator()
{
	AddComponents();
	ConnectSystemComponents();
}


void Emulator::AddComponents()
{
	components.push_back(&apu);
	components.push_back(&bus);
	components.push_back(&cartridge);
	components.push_back(&cpu);
	components.push_back(&ppu);
}


void Emulator::ConnectSystemComponents()
{
	bus.cartridge = &cartridge;
}


void Emulator::LoadState()
{
	std::ifstream ifs{ "state.bin" };
	if (!ifs)
	{
		wxMessageBox("Save state does not exist or could not be opened.");
		load_state_on_next_cycle = false;
		return;
	}

	for (Component* component : components)
		component->Deserialize(ifs);
	this->Deserialize(ifs);
}


void Emulator::SaveState()
{
	std::ofstream ofs{ "state.bin", std::ofstream::out };
	if (!ofs)
	{
		wxMessageBox("Save state could not be created.");
		save_state_on_next_cycle = false;
		return;
	}

	for (Component* component : components)
		component->Serialize(ofs);
	this->Serialize(ofs);
}


void Emulator::Serialize(std::ofstream& ofs)
{

}


void Emulator::Deserialize(std::ifstream& ifs)
{

}


void Emulator::StartGame(const char* rom_path)
{
	for (Component* component : components)
		component->Reset();

	bool rom_loaded = cartridge.ReadRomFile(rom_path);
	if (!rom_loaded) return;
	strcpy(this->current_rom_path, rom_path);

	MainLoop();
}


void Emulator::MainLoop()
{
	emu_is_running = true;
	emu_is_paused = false;
	long long microseconds_since_fps_update = 0; // how many microseconds since the fps window label was updated
	int frames_since_fps_update = 0; // same as above but no. of frames

	while (emu_is_running && !emu_is_paused)
	{
		auto frame_start_t = std::chrono::steady_clock::now();

		cycle_counter = 0;
		while (cycle_counter++ < cycles_per_frame_NTSC)
		{
			apu.Update();
			cpu.Update();
			ppu.Update();
		}

		if (load_state_on_next_cycle)
			LoadState();
		else if (save_state_on_next_cycle)
			SaveState();

		auto frame_end_t = std::chrono::steady_clock::now();
		long long microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();

		if (!emu_speed_uncapped)
		{
			while (microseconds_elapsed_this_frame < microseconds_per_frame_NTSC)
			{
				joypad.PollInput();
				microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frame_start_t).count();
			}
		}

		// update fps on window title if it is time to do so (updated once every second)
		frame_end_t = std::chrono::steady_clock::now();
		microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();
		frames_since_fps_update++;
		microseconds_since_fps_update += microseconds_elapsed_this_frame;
		if (microseconds_since_fps_update >= 1000000 && emu_is_running)
		{
			//gui->UpdateFPSLabel(frames_since_fps_update);
			frames_since_fps_update = 0;
			microseconds_since_fps_update -= 1000000;
		}
	}
}


void Emulator::Pause()
{
	emu_is_paused = true;
}


void Emulator::Reset()
{
	if (emu_is_running)
		cartridge.Eject();
	StartGame(this->current_rom_path);
}


void Emulator::Resume()
{
	if (emu_is_running)
		MainLoop();
}


void Emulator::Stop()
{
	if (emu_is_running)
		cartridge.Eject();
	emu_is_running = false;
}