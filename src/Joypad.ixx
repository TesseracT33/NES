export module Joypad;

import NumericalTypes;
import SerializationStream;

import <algorithm>;
import <array>;
import <cstring>;
import <utility>;

namespace Joypad
{
	export
	{
		enum class Button { 
			A, B, Select, Start, Up, Down, Left, Right
		};

		void NotifyButtonPressed(uint player_index, uint button_index);
		void NotifyButtonReleased(uint player_index, uint button_index);
		u8 PeekRegister(u16 addr);
		u8 ReadRegister(u16 addr);
		void Reset();
		void StreamState(SerializationStream& stream);
		void WriteRegister(u16 addr, u8 data);
	}

	bool strobe;
	bool strobe_seq_completed;

	struct Player
	{
		std::array<bool, 8> button_currently_held;
		std::array<bool, 8> button_held_on_last_latch;
		uint button_return_index; // The index of the button to be returned on the next read to register $4016/$4017
	};

	std::array<Player, 2> players;
}