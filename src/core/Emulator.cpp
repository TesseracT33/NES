#include "Emulator.h"


Emulator::Emulator()
{
	CreateComponentVector();
	ConnectSystemComponents();
}


void Emulator::CreateComponentVector()
{
	components.push_back(&apu);
	components.push_back(&bus);
	components.push_back(&cpu);
	components.push_back(&ppu);
}


void Emulator::ConnectSystemComponents()
{
	apu.cpu = &cpu;

	bus.apu = &apu;
	bus.cpu = &cpu;
	bus.joypad = &joypad;
	bus.ppu = &ppu;

	cpu.bus = &bus;

	ppu.bus = &bus;
	ppu.cpu = &cpu;
}


void Emulator::LoadState()
{
	if (!load_state_on_next_cycle)
	{
		load_state_on_next_cycle = true;
		return;
	}

	std::ifstream ifs(save_state_path, std::ifstream::in | std::ifstream::binary);
	if (!ifs)
	{
		wxMessageBox("Save state does not exist or could not be opened.");
		load_state_on_next_cycle = false;
		return;
	}

	Serialization::DeserializeFunctor functor{ ifs };
	for (Snapshottable* snapshottable : snapshottable_components)
		snapshottable->State(functor);

	ifs.close();
	load_state_on_next_cycle = false;
}


void Emulator::SaveState()
{
	if (!save_state_on_next_cycle)
	{
		save_state_on_next_cycle = true;
		return;
	}

	std::ofstream ofs(save_state_path, std::ofstream::out | std::ofstream::binary);
	if (!ofs)
	{
		wxMessageBox("Save state could not be created.");
		save_state_on_next_cycle = false;
		return;
	}

	Serialization::SerializeFunctor functor{ ofs };
	for (Snapshottable* snapshottable : snapshottable_components)
		snapshottable->State(functor);

	ofs.close();
	save_state_on_next_cycle = false;
}


void Emulator::State(Serialization::BaseFunctor& functor)
{

}


void Emulator::Configure(Serialization::BaseFunctor& functor)
{

}


void Emulator::SetDefaultConfig()
{

}


void Emulator::AddObserver(Observer* observer)
{
	this->gui = ppu.gui = observer;
}


bool Emulator::SetupSDLVideo(const void* window_handle)
{
	bool success = ppu.CreateRenderer(window_handle);
	return success;
}


void Emulator::SetEmulationSpeed(unsigned speed)
{
}


void Emulator::StartGame(std::string rom_path)
{
	// Construct a mapper class instance given the rom file. If it failed (e.g. if the mapper is not supported), return.
	std::optional<std::shared_ptr<BaseMapper>> mapper = Cartridge::ConstructMapperFromRom(rom_path);
	if (!mapper.has_value()) return;
	this->mapper = mapper.value();
	apu.mapper = bus.mapper = ppu.mapper = this->mapper.get();

	this->current_rom_path = rom_path;

	apu.Reset();
	bus.Reset();
	cpu.Power();
	ppu.Power();

	//std::thread t(&Emulator::MainLoop, this);
	//t.detach();
	run_cpu_init_cycles = true;
	MainLoop();
}


void Emulator::MainLoop()
{
	emu_is_running = true;
	emu_is_paused = false;
	long long microseconds_since_fps_update = 0; // how many microseconds since the fps window label was updated
	
	/* If this is the first time that MainLoop is called after starting a game, we run eight cpu cycles where the cpu is not executing any instructions. */
	if (run_cpu_init_cycles)
	{
		cpu.RunInitialCycles();
		run_cpu_init_cycles = false;
	}

	while (emu_is_running && !emu_is_paused)
	{
		auto frame_start_t = std::chrono::steady_clock::now();

		// Run the CPU for roughly 2/3 of a frame (exact timing is not important; audio/video synchronization is done by the APU).
		cpu.Run();

		joypad.PollInput();

		if (load_state_on_next_cycle)
			LoadState();
		else if (save_state_on_next_cycle)
			SaveState();

		auto frame_end_t = std::chrono::steady_clock::now();
		long long microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();

		//if (!emulation_speed_uncapped)
		//{
		//	while (microseconds_elapsed_this_frame < 11111)
		//	{
		//		joypad.PollInput();
		//		microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frame_start_t).count();
		//	}
		//}

		// update fps on window title if it is time to do so (updated once every second)
		frame_end_t = std::chrono::steady_clock::now();
		microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();
		microseconds_since_fps_update += microseconds_elapsed_this_frame;
		if (microseconds_since_fps_update >= 1000000 && emu_is_running)
		{
			gui->UpdateFPSLabel();
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
	StartGame(this->current_rom_path);
}


void Emulator::Resume()
{
	if (emu_is_running)
		MainLoop();
}


void Emulator::Stop()
{
	emu_is_running = false;
}