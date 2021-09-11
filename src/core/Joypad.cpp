#include "Joypad.h"


void Joypad::Reset()
{
	strobe = 0;
	for (int button = 0; button < num_buttons; button++)
	{
		for (int player = 0; player < 1; player++)
		{
			buttons_currently_held[button][player] = false;
			buttons_held_on_last_latch[button][player] = false;
		}
	}
	OpenGameControllers();
}


// Refers to the registers at addresses $4016 and $4017
u8 Joypad::ReadRegister(u16 addr)
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

	Player player;
	switch (addr)
	{
	case 0x4016: player = Player::ONE; break;
	case 0x4017: player = Player::TWO; break;
	default: break; // todo: throw exception
	}

	u8 ret;
	if (strobe == 1) // Case 3
		ret = buttons_currently_held[Button::A][player];
	else if (strobe_seq_completed) // Case 4
	{
		ret = button_return_index[player] < 8 ?
			buttons_currently_held[button_return_index[player]][player] : 1;
		button_return_index[player] = std::min(button_return_index[player] + 1, 8);
	}
	else // Case 1, 2
	{
		ret = 0;
	}

	// Every returned value is ANDed with $40, which is the upper byte of the address ($4016 or $4017)
	// From https://wiki.nesdev.com/w/index.php?title=Standard_controller: 
	//   "In the NES and Famicom, the top three (or five) bits are not driven, and so retain the bits of the previous byte on the bus. Usually this is the most significant byte of the address of the controller port—0x40"
	return ret | 0x40;
}


void Joypad::WriteRegister(u16 addr, u8 data)
{
	bool prev_strobe = strobe;
	strobe = data & 1;
	if (prev_strobe == 1 && strobe == 0)
	{
		memcpy(buttons_held_on_last_latch, buttons_currently_held, sizeof(bool) * num_buttons * 2);
		strobe_seq_completed = true;
	}
	else if (strobe_seq_completed && strobe == 0)
		return;
	else
	{
		strobe_seq_completed = false;
		button_return_index[0] = button_return_index[1] = 0;
	}
}


void Joypad::OpenGameControllers()
{
	// todo: make the controller detection system better at some point
	int num_joysticks_found = 0;
	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if (SDL_IsGameController(i))
		{
			controller[num_joysticks_found] = SDL_GameControllerOpen(i);
			if (controller != nullptr)
			{
				num_joysticks_found++;
				if (num_joysticks_found == 2)
					return;
			}
		}
	}
}


void Joypad::PollInput()
{
	if (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
			// TODO: identify the player number...
			MatchInputToBindings(event.key.keysym.sym, InputEvent::PRESS, InputMethod::KEYBOARD, Player::ONE);
			break;

		case SDL_KEYUP:
			MatchInputToBindings(event.key.keysym.sym, InputEvent::RELEASE, InputMethod::KEYBOARD, Player::ONE);
			break;

		case SDL_CONTROLLERBUTTONDOWN:
			MatchInputToBindings(event.cbutton.button, InputEvent::PRESS, InputMethod::JOYPAD, Player::ONE);
			break;

		case SDL_CONTROLLERBUTTONUP:
			MatchInputToBindings(event.cbutton.button, InputEvent::RELEASE, InputMethod::JOYPAD, Player::ONE);
			break;

		case SDL_CONTROLLERDEVICEADDED:
			OpenGameControllers();
			break;

		case SDL_CONTROLLERDEVICEREMOVED:
			OpenGameControllers();
			break;

		default:
			break;
		}
	}
}


void Joypad::UpdateBinding(Button button, SDL_GameControllerButton bind, Player player)
{
	new_bindings[button][player] = Bind(bind, InputMethod::JOYPAD);
}


void Joypad::UpdateBinding(Button button, SDL_Keycode key, Player player)
{
	new_bindings[button][player] = Bind(key, InputMethod::KEYBOARD);
}


void Joypad::SaveBindings()
{
	for (int button = 0; button < num_buttons; button++)
	{
		for (int player = 0; player < 1; player++)
		{
			bindings[button][player] = new_bindings[button][player];
		}
	}
}


void Joypad::RevertBindingChanges()
{
	for (int button = 0; button < num_buttons; button++)
	{
		for (int player = 0; player < 1; player++)
		{
			new_bindings[button][player] = bindings[button][player];
		}
	}
}


void Joypad::ResetBindings(Player player)
{
	for (int button = 0; button < num_buttons; button++)
		new_bindings[button][player] = Bind(default_keyboard_bindings[button]);
}


const char* Joypad::GetCurrentBindingString(Button button, Player player)
{
	if (new_bindings[button][player].type == InputMethod::JOYPAD)
		return SDL_GameControllerGetStringForButton((SDL_GameControllerButton)new_bindings[button][player].button);
	else
		return SDL_GetKeyName((SDL_KeyCode)new_bindings[button][player].button);
}


void Joypad::MatchInputToBindings(s32 button, InputEvent input_event, InputMethod input_method, Player player)
{
	// 'button' can either be a SDL_Keycode (typedef s32) or an u8 (from a controller button event)
	for (int i = 0; i < num_buttons; i++)
	{
		if (button == bindings[i][player].button && bindings[i][player].type == input_method)
		{
			buttons_currently_held[i][player] = input_event;
			return;
		}
	}
}


void Joypad::Configure(Serialization::BaseFunctor& functor)
{
	functor.fun(bindings, sizeof Bind * num_buttons * 2);
}


void Joypad::SetDefaultConfig()
{
	for (int button = 0; button < num_buttons; button++)
	{
		for (int player = 0; player < 1; player++)
		{
			bindings[button][player] = default_keyboard_bindings[button];
		}
	}
}