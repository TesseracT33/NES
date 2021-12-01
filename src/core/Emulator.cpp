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

	SerializationStream stream{ save_state_path, SerializationStream::Mode::Deserialization };
	if (stream.HasError())
	{
		UserMessage::Show("Save state does not exist or could not be opened.", UserMessage::Type::Error);
		load_state_on_next_cycle = false;
		return;
	}

	for (Snapshottable* snapshottable : snapshottable_components)
		snapshottable->StreamState(stream);

	if (stream.HasError())
	{
		UserMessage::Show("Save state does not exist or could not be opened.", UserMessage::Type::Error);
		load_state_on_next_cycle = false;
		return;
	}

	load_state_on_next_cycle = false;
}


void Emulator::SaveState()
{
	if (!save_state_on_next_cycle)
	{
		save_state_on_next_cycle = true;
		return;
	}

	SerializationStream stream{ save_state_path, SerializationStream::Mode::Deserialization };
	if (stream.HasError())
	{
		UserMessage::Show("Save state could not be created.", UserMessage::Type::Error);
		save_state_on_next_cycle = false;
		return;
	}

	for (Snapshottable* snapshottable : snapshottable_components)
		snapshottable->StreamState(stream);

	if (stream.HasError())
	{
		UserMessage::Show("Save state could not be created.", UserMessage::Type::Error);
		save_state_on_next_cycle = false;
		return;
	}

	save_state_on_next_cycle = false;
}


void Emulator::StreamState(SerializationStream& stream)
{

}


void Emulator::StreamConfig(SerializationStream& stream)
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

	/* The operations of the apu and ppu are affected by the video standard (NTSC/PAL/Dendy). */
	const System::VideoStandard video_standard = mapper.get()->GetVideoStandard();
	nes.apu->PowerOn(video_standard);
	nes.cpu->PowerOn();
	nes.ppu->PowerOn(video_standard);

	/* Read potential save data */
	nes.mapper->ReadPRGRAMFromDisk();

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

	long long milliseconds_since_fps_update = 0; // how many milliseconds since the fps window label was updated
	long long milliseconds_since_save_data_flushed_to_disk = 0;

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
		long long milliseconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end_t - frame_start_t).count();

		// update fps on window title if it is time to do so
		milliseconds_since_fps_update += milliseconds_elapsed;
		const long long milliseconds_per_fps_update = 1000;
		if (milliseconds_since_fps_update >= milliseconds_per_fps_update && emu_is_running)
		{
			gui->UpdateFPSLabel();
			milliseconds_since_fps_update -= milliseconds_per_fps_update;
		}

		milliseconds_since_save_data_flushed_to_disk += milliseconds_elapsed;
		const long long milliseconds_per_save_data_flush = 5000;
		if (milliseconds_since_save_data_flushed_to_disk >= milliseconds_per_save_data_flush && emu_is_running)
		{
			nes.mapper->WritePRGRAMToDisk();
			milliseconds_since_save_data_flushed_to_disk -= milliseconds_per_save_data_flush;
		}
	}
}


void Emulator::Pause()
{
	emu_is_paused = true;
}


void Emulator::Reset()
{
	nes.apu->Reset();
	nes.bus->Reset();
	nes.cpu->Reset();
	nes.ppu->Reset();
	MainLoop();
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