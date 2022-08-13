export module NES;

import APU;
import Bus;
import Cartridge;
import CPU;
import Joypad;
import PPU;
import System;

import NumericalTypes;
import SerializationStream;

import <string>;

export namespace NES
{
	void ApplyNewSampleRate()
	{
		APU::ApplyNewSampleRate();
	}


	bool AssociatesWithRomExtension(const std::string& ext)
	{
		return ext.compare("nes") == 0 || ext.compare("NES") == 0;
	}


	void Detach()
	{
		Cartridge::Eject();
	}


	void DisableAudio()
	{
		// TODO
	}


	void EnableAudio()
	{
		// TODO
	}


	uint GetNumberOfInputs()
	{
		return 8;
	}


	void Initialize()
	{
		APU::PowerOn();
		Bus::PowerOn();
		CPU::PowerOn();
		Joypad::Reset();
		PPU::PowerOn();

		CPU::RunStartUpCycles();
	}


	bool LoadBios(const std::string& path)
	{
		return true; /* no bios */
	}


	bool LoadRom(const std::string& path)
	{
		return Cartridge::LoadRom(path);
	}


	void NotifyNewAxisValue(uint player_index, uint input_action_index, int axis_value)
	{
		/* no axes */
	}


	void NotifyButtonPressed(uint player_index, uint input_action_index)
	{
		Joypad::NotifyButtonPressed(player_index, input_action_index);
	}


	void NotifyButtonReleased(uint player_index, uint input_action_index)
	{
		Joypad::NotifyButtonReleased(player_index, input_action_index);
	}


	void Reset()
	{
		APU::Reset();
		CPU::Reset();
		Joypad::Reset();
		PPU::Reset();
	}


	void Run()
	{
		CPU::Run();
	}


	void StreamState(SerializationStream& stream)
	{
		APU::StreamState(stream);
		Bus::StreamState(stream);
		Cartridge::StreamState(stream);
		CPU::StreamState(stream);
		Joypad::StreamState(stream);
		PPU::StreamState(stream);
		System::StreamState(stream);
	}
}