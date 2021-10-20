#pragma once

#include <SDL.h>
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
	static const int num_input_keys = 8;

	enum ButtonID
	{
		bind_start_p1            = 20000                         ,
		bind_start_p2            = bind_start_p1 + num_input_keys,
		set_to_keyboard_defaults = bind_start_p2 + num_input_keys,
		set_to_joypad_defaults                                   ,
		cancel_and_exit                                          ,
		save_and_exit                                            ,
		unbind_p1                                                ,
		unbind_p2
	};

	const int padding = 10; // space (pixels) between some controls
	const wxSize button_bind_size      = wxSize( 70,  25);
	const wxSize button_options_size   = wxSize(150,  25);
	const wxSize controller_image_size = wxSize(384, 206);
	const wxSize label_size            = wxSize( 50,  25);
	const wxString controller_image_path = "resources//nes_controller_front.png";
	const wxString button_labels[num_input_keys] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN" };

	bool awaiting_input = false; /* The user has pressed a bind button, and we are waiting for input. */
	bool* window_active = nullptr;
	int index_of_awaited_input_button;

	Config* config = nullptr;
	Joypad* joypad = nullptr;

	wxButton* buttons_p1[num_input_keys]{};
	wxButton* buttons_p2[num_input_keys]{};
	wxButton* button_set_to_keyboard_defaults = nullptr;
	wxButton* button_set_to_joypad_defaults = nullptr;
	wxButton* button_save_and_exit = nullptr;
	wxButton* button_cancel_and_exit = nullptr;
	wxButton* button_unbind_p1 = nullptr;
	wxButton* button_unbind_p2 = nullptr;

	wxStaticBitmap* controller_image = nullptr;

	wxStaticText* static_text_buttons[num_input_keys]{};
	wxStaticText* static_text_control = nullptr;
	wxStaticText* static_text_bind_p1 = nullptr;
	wxStaticText* static_text_bind_p2 = nullptr;
	
	wxString prev_input_button_label;

	// event methods
	void OnInputButtonPress(wxCommandEvent& event);
	void OnResetKeyboard(wxCommandEvent& event);
	void OnResetJoypad(wxCommandEvent& event);
	void OnCancelAndExit(wxCommandEvent& event);
	void OnSaveAndExit(wxCommandEvent& event);
	void OnUnbindAll(wxCommandEvent& event);
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