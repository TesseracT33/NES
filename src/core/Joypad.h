#pragma once

#include "SDL.h"
#include "SDL_gamecontroller.h"

#include "../Types.h"

#include "Component.h"

class Joypad final : public Component
{
public:
	enum class InputMethod { JOYPAD, KEYBOARD };
	enum Button { A, B, SELECT, START, UP, DOWN, LEFT, RIGHT };
	enum InputEvent { RELEASE = 0, PRESS = 1 };
	enum Player { ONE = 0, TWO = 1 };

	u8 ReadRegister(u16 addr);
	void WriteRegister(u16 addr, u8 data);

	const char* GetCurrentBindingString(Button button, Player player);
	void OpenGameControllers();
	void PollInput();
	void Reset();
	void ResetBindings(Player player);
	void RevertBindingChanges();
	void SaveBindings();
	void UnbindAll(Player player);
	void UpdateBinding(Button button, SDL_GameControllerButton bind, Player player);
	void UpdateBinding(Button button, SDL_Keycode key, Player player);

	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

private:
	struct Bind
	{
		s32 button; // Either an SDL_Keycode (typedef s32) or a u8 (from a controller button event)
		InputMethod type;

		Bind() = default;
		Bind(s32 _button, InputMethod _type)  : button(_button), type(_type) {}
		Bind(SDL_Keycode keycode)              : button(keycode) { type = InputMethod::KEYBOARD; }
		Bind(SDL_GameControllerButton _button) : button(_button) { type = InputMethod::JOYPAD; }
	};

	static const unsigned num_buttons = 8;

	bool strobe = 0;
	bool strobe_seq_completed = false;
	
	int button_return_index[2] = { 0, 0 }; // The index of the button to be returned on the next read to this register, for each player

	SDL_GameController* controller[2];
	SDL_Joystick* joystick[2];
	SDL_Event event;

	bool buttons_currently_held[num_buttons][2]{}; // the 2nd dimension is for the two players
	bool buttons_held_on_last_latch[num_buttons][2]{};

	Bind bindings[num_buttons][2], new_bindings[num_buttons][2];

	const SDL_Keycode default_keyboard_bindings[num_buttons] =
	{ SDLK_SPACE, SDLK_LCTRL, SDLK_BACKSPACE, SDLK_RETURN, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };

	const SDL_GameControllerButton default_joypad_bindings[num_buttons] =
	{ SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
	SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT };

	void MatchInputToBindings(s32 button, InputEvent input_event, InputMethod input_method, Player player);
};