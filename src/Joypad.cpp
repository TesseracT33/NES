module Joypad;

namespace Joypad
{
	void NotifyButtonPressed(uint player_index, uint button_index)
	{
		if (player_index <= 2) {
			players[player_index].button_currently_held[button_index] = true;
		}
	}


	void NotifyButtonReleased(uint player_index, uint button_index)
	{
		if (player_index <= 2) {
			players[player_index].button_currently_held[button_index] = false;
		}
	}


	u8 PeekRegister(u16 addr)
	{
		/* See 'ReadRegister' for behavior */
		Player& player = players[addr - 0x4016];

		return 0x40 | [&] {
			if (strobe == 1) {
				static constexpr auto index_a = std::to_underlying(Button::A);
				return static_cast<int>(player.button_currently_held[index_a]);
			}
			else if (strobe_seq_completed) {
				return player.button_return_index < 8 ?
					player.button_currently_held[player.button_return_index] : 1;
			}
			else {
				return 0;
			}
		}();
	}


	// Refers to the registers at addresses $4016 and $4017
	u8 ReadRegister(u16 addr)
	{
		/* Behaviour when reading either $4016 or $4017, dependent on the recent history of what has been written to the LSB of $4016.
		   '...' represents arbitrary bits written prior, and '*' is any number of duplication of the bit written before it.
		   Source: http://archive.nes.science/nesdev-forums/f2/t1637.xhtml
		   - Nothing  : Return 0 on every read. The actual behaviour is the same as the case '...10*', but the latched controller state should be that all buttons are inactive.
		   - 0*       : Same as above.
		   - ...1*    : Return the active status of the A button (as the LSB in the returned byte)
		   - ...10*   : Return the status of the A button as it was when the 0 was written, then the same for B on the next read, etc.
						The full order is : A, B, SELECT, START, UP, DOWN, LEFT, RIGHT
						After eight reads, 0x01 is returned on every following read
		*/

		Player& player = players[addr - 0x4016];

		return 0x40 | [&] {
			if (strobe == 1) { // Case 3
				static constexpr auto index_a = std::to_underlying(Button::A);
				return static_cast<int>(player.button_currently_held[index_a]);
			}
			else if (strobe_seq_completed) { // Case 4
				int ret = player.button_return_index < 8 ?
					player.button_currently_held[player.button_return_index] : 1;
				player.button_return_index = std::min(int(player.button_return_index + 1), 8);
				return ret;
			}
			else { // Case 1, 2
				return 0;
			}
		}();
		// Every returned value is ANDed with $40, which is the upper byte of the address ($4016 or $4017)
		// From https://wiki.nesdev.com/w/index.php?title=Standard_controller: 
		//   "In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. 
		//    Usually this is the most significant byte of the address of the controller port—0x40"
	}


	void Reset()
	{
		strobe = 0;
		strobe_seq_completed = false;
		std::memset(players.data(), 0, sizeof(players));
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamArray(players);
		stream.StreamPrimitive(strobe);
		stream.StreamPrimitive(strobe_seq_completed);
	}


	void WriteRegister(u16 addr, u8 data)
	{
		auto prev_strobe = strobe;
		strobe = data & 1;
		if (prev_strobe == 1 && strobe == 0) {
			for (int i = 0; i < players.size(); ++i) {
				std::copy(
					players[i].button_currently_held.begin(),
					players[i].button_currently_held.end(),
					players[i].button_held_on_last_latch.begin()
				);
			}
			strobe_seq_completed = true;
		}
		else if (!strobe_seq_completed || strobe != 0) {
			strobe_seq_completed = false;
			players[0].button_return_index = players[1].button_return_index = 0;
		}
	}
}