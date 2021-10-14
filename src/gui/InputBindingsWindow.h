#pragma once

#include <wx/wx.h>
#include <wx/button.h>
#include <wx/log.h>
#include <wx/stattext.h>

#include "../Config.h"
#include "../core/Emulator.h"

class InputBindingsWindow : public wxFrame
{
public:
	InputBindingsWindow(wxWindow* parent, Config* config, Joypad* joypad, bool* window_active);
	// note: no destructor where all heap-allocated objects are deleted is needed. These are automatically destroyed when the window is destroyed

private:
	Config* config = nullptr;
	Joypad* joypad = nullptr;

	bool* window_active = nullptr;

	const wxSize button_size = wxSize(70, 25);
	const wxSize button_options_size = wxSize(150, 25);
	const wxSize label_size = wxSize(50, 25);
	static const int padding = 10; // space (pixels) between controls

	static const int NUM_INPUT_KEYS = 8;
	static const int ID_BUTTON_BASE_P1 = 20000;
	static const int ID_BUTTON_BASE_P2 = ID_BUTTON_BASE_P1 + NUM_INPUT_KEYS;
	static const int ID_SET_TO_KEYBOARD_DEFAULT = ID_BUTTON_BASE_P2 + NUM_INPUT_KEYS;
	static const int ID_SET_TO_JOYPAD_DEFAULT = ID_SET_TO_KEYBOARD_DEFAULT + 1;
	static const int ID_CANCEL_AND_EXIT = ID_SET_TO_KEYBOARD_DEFAULT + 2;
	static const int ID_SAVE_AND_EXIT = ID_SET_TO_KEYBOARD_DEFAULT + 3;

	const char* button_labels[NUM_INPUT_KEYS] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN" };

	wxButton* buttons_p1[NUM_INPUT_KEYS]{};
	wxButton* buttons_p2[NUM_INPUT_KEYS]{};
	wxButton* button_set_to_keyboard_defaults = nullptr;
	wxButton* button_set_to_joypad_defaults = nullptr;
	wxButton* button_save_and_exit = nullptr;
	wxButton* button_cancel_and_exit = nullptr;

	wxStaticText* static_text_buttons[NUM_INPUT_KEYS]{};
	wxStaticText* static_text_control = nullptr;
	wxStaticText* static_text_bind_p1 = nullptr;
	wxStaticText* static_text_bind_p2 = nullptr;

	/// NES controller front image ///
	const unsigned image_width = 384;
	const unsigned image_height = 206;
	const wxString image_path = "resources//nes_controller_front.png";
	wxStaticBitmap* image = nullptr;

	int index_of_awaited_input_button;
	bool awaiting_input = false;
	wxString prev_input_button_label;

	// event methods
	void OnInputButtonPress(wxCommandEvent& event);
	void OnResetKeyboard(wxCommandEvent& event);
	void OnResetJoypad(wxCommandEvent& event);
	void OnCancelAndExit(wxCommandEvent& event);
	void OnSaveAndExit(wxCommandEvent& event);
	void OnCloseWindow(wxCloseEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void OnJoyDown(wxJoystickEvent& event);
	void OnButtonLostFocus(wxFocusEvent& event);

	void GetAndSetButtonLabels();
	void CheckForDuplicateBindings(const char* new_bound_key_name);
	SDL_Keycode Convert_WX_Keycode_To_SDL_Keycode(int wx_keycode);
	u8 Convert_WX_Joybutton_To_SDL_Joybutton(int wx_joybutton);

	wxDECLARE_EVENT_TABLE();
};