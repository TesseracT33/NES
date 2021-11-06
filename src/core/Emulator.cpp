#include "Emulator.h"


Emulator::Emulator()
{
	ConstructNES();
	CreateSnapshottableComponentVector();
}


void Emulator::CreateSnapshottableComponentVector()
{
	// TODO
}


void Emulator::ConstructNES()
{
	nes.apu    = std::make_unique<APU>    (&nes);
	nes.bus    = std::make_unique<BusImpl>(&nes);
	nes.cpu    = std::make_unique<CPU>    (&nes);
	nes.joypad = std::make_unique<Joypad> (&nes);
	nes.ppu    = std::make_unique<PPU>    (&nes);
	/* Note: the mapper will be created when a game is loaded. */
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
		UserMessage::Show("Save state does not exist or could not be opened.", UserMessage::Type::Error);
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
		UserMessage::Show("Save state could not be created.", UserMessage::Type::Error);
		save_state_on_next_cycle = false;
		return;
	}

	Serialization::SerializeFunctor functor{ ofs };
	for (Snapshottable* snapshottable : snapshottable_components)
		snapshottable->State(functor);

	ofs.close();
	save_state_on_next_cycle = false;
}


void Emulator::State(Serialization::Functor& functor)
{

}


void Emulator::Configure(Serialization::Functor& functor)
{

}


void Emulator::SetDefaultConfig()
{

}


void Emulator::AddObserver(Observer* observer)
{
	this->gui = nes.ppu->gui = observer;
}


bool Emulator::SetupSDLVideo(const void* window_handle)
{
	bool success = nes.ppu->CreateRenderer(window_handle);
	return success;
}


void Emulator::SetEmulationSpeed(unsigned speed)
{
}


/* Returns true on success, otherwise false. */
bool Emulator::PrepareLaunchOfGame(const std::string& rom_path)
{
	// Construct a mapper class instance given the rom file. If it failed (e.g. if the mapper is not supported), return.
	std::optional<std::shared_ptr<BaseMapper>> mapper_opt = Cartridge::ConstructMapperFromRom(rom_path);
	if (!mapper_opt.has_value()) return false;
	std::shared_ptr<BaseMapper> mapper = mapper_opt.value();
	nes.mapper = mapper;
	mapper->AttachNES(&nes);

	this->current_rom_path = rom_path;

	nes.bus->Reset();
	nes.cpu->PowerOn();

	/* The operations of the apu and ppu are affected by the video standard (NTSC/PAL/Dendy). */
	const System::VideoStandard video_standard = mapper.get()->GetVideoStandard();
	nes.apu->PowerOn(video_standard);
	nes.ppu->PowerOn(video_standard);

	return true;
}


void Emulator::LaunchGame()
{
	nes.cpu->RunStartUpCycles();
	MainLoop();
}


void Emulator::MainLoop()
{
	emu_is_running = true;
	emu_is_paused = false;
	long long microseconds_since_fps_update = 0; // how many microseconds since the fps window label was updated

	while (emu_is_running && !emu_is_paused)
	{
		auto frame_start_t = std::chrono::steady_clock::now();

		// Run the CPU for roughly 2/3 of a frame (exact timing is not important; audio/video synchronization is done by the APU).
		try {
			nes.cpu->Run();
		}
		catch (const std::runtime_error& e)
		{
			UserMessage::Show(e.what(), UserMessage::Type::Error);
		}

		nes.joypad->PollInput();

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
	PrepareLaunchOfGame(this->current_rom_path);
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