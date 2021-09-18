#include "MainWindow.h"


wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
	EVT_MENU(MenuBarID::open_rom, MainWindow::OnMenuOpen)
	EVT_MENU(MenuBarID::set_game_dir, MainWindow::OnMenuSetGameDir)
	EVT_MENU(MenuBarID::save_state, MainWindow::OnMenuSaveState)
	EVT_MENU(MenuBarID::load_state, MainWindow::OnMenuLoadState)
	EVT_MENU(MenuBarID::quit, MainWindow::OnMenuQuit)
	EVT_MENU(MenuBarID::pause_play, MainWindow::OnMenuPausePlay)
	EVT_MENU(MenuBarID::reset, MainWindow::OnMenuReset)
	EVT_MENU(MenuBarID::stop, MainWindow::OnMenuStop)
	EVT_MENU(MenuBarID::size_1x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_2x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_4x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_6x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_8x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_10x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_12x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_15x, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_custom, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_maximized, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::size_fullscreen, MainWindow::OnMenuSize)
	EVT_MENU(MenuBarID::speed_50, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_75, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_100, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_125, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_150, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_200, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_300, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_500, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_uncapped, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_custom, MainWindow::OnMenuSpeed)
	EVT_MENU(MenuBarID::input, MainWindow::OnMenuInput)
	EVT_MENU(MenuBarID::toggle_filter_nes_files, MainWindow::OnMenuToggleFilterFiles)
	EVT_MENU(MenuBarID::reset_settings, MainWindow::OnMenuResetSettings)
	EVT_MENU(MenuBarID::github_link, MainWindow::OnMenuGitHubLink)

	EVT_LISTBOX_DCLICK(listBoxID, MainWindow::OnListBoxGameSelection)
	EVT_SIZE(MainWindow::OnWindowSizeChanged)
	EVT_KEY_DOWN(MainWindow::OnKeyDown)
	EVT_CLOSE(MainWindow::OnClose)
wxEND_EVENT_TABLE()


#define PRESPECIFIED_SPEED_NOT_FOUND -1
#define PRESPECIFIED_SIZE_NOT_FOUND -1


const wxString MainWindow::emulator_name("nes-dono");


MainWindow::MainWindow() : wxFrame(nullptr, frameID, emulator_name, wxDefaultPosition, wxDefaultSize)
{
	SetupInitialMenuView();
	SwitchToMenuView();

	SetupConfig();
	ApplyGUISettings();

	emulator.AddObserver(this);

	SetupSDL();
}


MainWindow::~MainWindow()
{
	delete game_list_box;
	delete SDL_window_panel;
}


void MainWindow::SetupInitialMenuView()
{
	SetClientSize(default_window_size);
	CreateMenuBar();
	default_client_size = GetClientSize(); // what remains of the window outside of the menubar

	game_list_box = new wxListBox(this, listBoxID, wxPoint(0, 0), default_client_size);
	SDL_window_panel = new wxPanel(this, SDLWindowID, wxPoint(0, 0), default_client_size);

	game_list_box->Bind(wxEVT_KEY_DOWN, &MainWindow::OnKeyDown, this);
	SDL_window_panel->Bind(wxEVT_KEY_DOWN, &MainWindow::OnKeyDown, this);
}


void MainWindow::SwitchToMenuView()
{
	SDL_window_panel->Hide();
	game_list_box->Show();
	SetClientSize(default_client_size);
	UpdateWindowLabel(false);
	menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
	game_view_active = false;
}


void MainWindow::SwitchToGameView()
{
	game_list_box->Hide();
	SDL_window_panel->Show();
	SDL_window_panel->SetFocus();
	wxSize SDL_window_size = emulator.GetWindowSize();
	SDL_window_panel->SetSize(SDL_window_size);
	SetClientSize(SDL_window_size);
	UpdateWindowLabel(true);
	game_view_active = true;
}


void MainWindow::SetupConfig()
{
	config.AddConfigurable(this);
	config.AddConfigurable(&emulator);
	config.AddConfigurable(&emulator.joypad);
	config.AddConfigurable(&emulator.ppu);

	if (config.ConfigFileExists())
		config.Load();
	else
		config.SetDefaults();
}


void MainWindow::LaunchGame()
{
	if (!wxFileExists(active_rom_path))
	{
		wxMessageBox(wxString::Format("Error opening game; file \"%s\" does not exist.", active_rom_path));
		return;
	}

	if (!SDL_initialized)
	{
		bool success = SetupSDL();
		SDL_initialized = success;
		if (!success)
			return;
	}

	SwitchToGameView();
	emulator.StartGame(std::string(active_rom_path.mb_str()));
}


void MainWindow::QuitGame()
{
	emulator.Stop();
	SwitchToMenuView();
}


// returns true if successful
bool MainWindow::SetupSDL()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
	{
		wxMessageBox("Could not initialise SDL!");
		return false;
	}

	const void* window_handle = (void*)SDL_window_panel->GetHandle();
	bool success = emulator.SetupSDLVideo(window_handle);
	SDL_initialized = success;
	return success;
}


void MainWindow::CreateMenuBar()
{
	menu_bar->Append(menu_file, wxT("&File"));
	menu_bar->Append(menu_emulation, wxT("&Emulation"));
	menu_bar->Append(menu_settings, wxT("&Settings"));
	menu_bar->Append(menu_info, wxT("&Info"));

	menu_file->Append(MenuBarID::open_rom, wxT("&Open rom"));
	menu_file->Append(MenuBarID::set_game_dir, wxT("&Set game directory"));
	menu_file->AppendSeparator();
	menu_file->Append(MenuBarID::save_state, wxT("&Save state (F5)"));
	menu_file->Append(MenuBarID::load_state, wxT("&Load state (F8)"));
	menu_file->AppendSeparator();
	menu_file->Append(MenuBarID::quit, wxT("&Quit"));

	menu_emulation->Append(MenuBarID::pause_play, wxT("&Pause"));
	menu_emulation->Append(MenuBarID::reset, wxT("&Reset"));
	menu_emulation->Append(MenuBarID::stop, wxT("&Stop"));

	menu_size->AppendRadioItem(MenuBarID::size_1x, FormatSizeMenubarLabel(1));
	menu_size->AppendRadioItem(MenuBarID::size_2x, FormatSizeMenubarLabel(2));
	menu_size->AppendRadioItem(MenuBarID::size_4x, FormatSizeMenubarLabel(4));
	menu_size->AppendRadioItem(MenuBarID::size_6x, FormatSizeMenubarLabel(6));
	menu_size->AppendRadioItem(MenuBarID::size_8x, FormatSizeMenubarLabel(8));
	menu_size->AppendRadioItem(MenuBarID::size_10x, FormatSizeMenubarLabel(10));
	menu_size->AppendRadioItem(MenuBarID::size_12x, FormatSizeMenubarLabel(12));
	menu_size->AppendRadioItem(MenuBarID::size_15x, FormatSizeMenubarLabel(15));
	menu_size->AppendRadioItem(MenuBarID::size_custom, wxT("&Custom"));
	menu_size->Append(MenuBarID::size_maximized, wxT("&Toggle maximized window"));
	menu_size->AppendSeparator();
	menu_size->Append(MenuBarID::size_fullscreen, wxT("&Toggle fullscreen (F11)"));
	menu_settings->AppendSubMenu(menu_size, wxT("&Window size"));

	menu_speed->AppendRadioItem(MenuBarID::speed_50, FormatSpeedMenubarLabel(50));
	menu_speed->AppendRadioItem(MenuBarID::speed_75, FormatSpeedMenubarLabel(75));
	menu_speed->AppendRadioItem(MenuBarID::speed_100, FormatSpeedMenubarLabel(100));
	menu_speed->AppendRadioItem(MenuBarID::speed_125, FormatSpeedMenubarLabel(125));
	menu_speed->AppendRadioItem(MenuBarID::speed_150, FormatSpeedMenubarLabel(150));
	menu_speed->AppendRadioItem(MenuBarID::speed_200, FormatSpeedMenubarLabel(200));
	menu_speed->AppendRadioItem(MenuBarID::speed_300, FormatSpeedMenubarLabel(300));
	menu_speed->AppendRadioItem(MenuBarID::speed_500, FormatSpeedMenubarLabel(500));
	menu_speed->AppendRadioItem(MenuBarID::speed_uncapped, wxT("&Uncapped"));
	menu_speed->AppendRadioItem(MenuBarID::speed_custom, wxT("&Custom"));
	menu_settings->AppendSubMenu(menu_speed, wxT("&Emulation speed"));

	menu_settings->Append(MenuBarID::input, wxT("&Configure input bindings"));

	menu_settings->AppendSeparator();
	menu_settings->AppendCheckItem(MenuBarID::toggle_filter_nes_files, wxT("&Filter game list to .nes files only"));

	menu_settings->AppendSeparator();
	menu_settings->Append(MenuBarID::reset_settings, wxT("&Reset settings"));

	menu_info->Append(MenuBarID::github_link, wxT("&GitHub link"));

	SetMenuBar(menu_bar);
}


// fill the game list with the games in specified rom directory
void MainWindow::SetupGameList()
{
	rom_folder_name_arr.Clear();
	rom_folder_path_arr.Clear();
	game_list_box->Clear();

	wxString filespec = wxEmptyString;
	if (menu_settings->IsChecked(MenuBarID::toggle_filter_nes_files))
		filespec = "*.nes";
	size_t num_files = wxDir::GetAllFiles(rom_folder_path, &rom_folder_path_arr, filespec, wxDIR_FILES);

	if (num_files == 0)
	{
		game_list_box->InsertItems(1, &empty_listbox_item, 0);
		return;
	}

	for (size_t i = 0; i < num_files; i++)
		rom_folder_name_arr.Add(wxFileName(rom_folder_path_arr[i]).GetFullName());
	game_list_box->InsertItems(rom_folder_name_arr, 0);
}


void MainWindow::ApplyGUISettings()
{
	menu_settings->Check(MenuBarID::toggle_filter_nes_files, display_only_nes_files);

	SetupGameList();

	unsigned scale = emulator.GetWindowScale();
	int menu_id = GetIdOfSizeMenubarItem(scale);
	if (menu_id == wxNOT_FOUND)
	{
		menu_size->Check(MenuBarID::size_custom, true);
		menu_size->SetLabel(MenuBarID::size_custom, FormatCustomSizeMenubarLabel(scale));
	}
	else
	{
		menu_size->Check(menu_id, true);
	}

	if (emulator.emulation_speed_uncapped)
	{
		menu_speed->Check(MenuBarID::speed_uncapped, true);
	}
	else
	{
		unsigned speed = emulator.emulation_speed;
		menu_id = GetIdOfSpeedMenubarItem(speed);
		if (menu_id == wxNOT_FOUND)
		{
			menu_speed->Check(MenuBarID::speed_custom, true);
			menu_speed->SetLabel(MenuBarID::speed_custom, FormatCustomSpeedMenubarLabel(speed));
		}
		else
		{
			menu_speed->Check(menu_id, true);
		}
	}
}


void MainWindow::Quit()
{
	QuitGame();
	SDL_Quit();
	Close();
}


void MainWindow::UpdateWindowLabel(bool gameIsRunning)
{
	wxSize window_size = GetClientSize();
	int scale = std::min(window_size.GetWidth() / 256, window_size.GetHeight() / 240); // TODO: hardcoded; fix

	if (gameIsRunning)
		this->SetLabel(wxString::Format("%s | %s | %ix%i | FPS: %i",
			emulator_name, wxFileName(active_rom_path).GetName(), 256 * scale, 240 * scale, frames_since_update));
	else
		this->SetLabel(emulator_name);
}


void MainWindow::OnMenuOpen(wxCommandEvent& event)
{
	// Creates an "open file" dialog
	wxFileDialog* fileDialog = new wxFileDialog(
		this, "Choose a file to open", wxEmptyString,
		wxEmptyString, "NES files (*.nes)|*.nes|All files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = fileDialog->ShowModal(); // show the dialog
	wxString selectedPath = fileDialog->GetPath();
	fileDialog->Destroy();

	// if the user clicks "Open" instead of "Cancel"
	if (buttonPressed == wxID_OK)
	{
		active_rom_path = selectedPath;
		LaunchGame();
	}
}


void MainWindow::OnMenuSetGameDir(wxCommandEvent& event)
{
	ChooseGameDirDialog();
}


void MainWindow::ChooseGameDirDialog()
{
	// Creates a "select directory" dialog
	wxDirDialog* dirDialog = new wxDirDialog(this, "Choose a directory",
		wxEmptyString, wxDD_DIR_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = dirDialog->ShowModal();
	wxString selectedPath = dirDialog->GetPath();
	dirDialog->Destroy();

	if (buttonPressed == wxID_OK)
	{
		// set the chosen directory
		rom_folder_path = selectedPath;
		SetupGameList();
		config.Save();
	}
}


void MainWindow::OnMenuSaveState(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
		emulator.SaveState();
	else
		wxMessageBox("No game is loaded. Cannot save state.");
}


void MainWindow::OnMenuLoadState(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
		emulator.LoadState();
	else
		wxMessageBox("No game is loaded. Cannot load a state.");
}


void MainWindow::OnMenuQuit(wxCommandEvent& event)
{
	Quit();
}


void MainWindow::OnMenuPausePlay(wxCommandEvent& event)
{
	if (emulator.emu_is_paused)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
		emulator.Resume();
	}
	else if (emulator.emu_is_running)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Resume"));
		emulator.Pause();
	}
}


void MainWindow::OnMenuReset(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
		emulator.Reset();
	}
}


void MainWindow::OnMenuStop(wxCommandEvent& event)
{
	QuitGame();
}


void MainWindow::OnMenuSize(wxCommandEvent& event)
{
	int id = event.GetId();
	int scale = GetSizeFromMenuBarID(id);

	if (scale != PRESPECIFIED_SIZE_NOT_FOUND)
	{
		if (menu_size->IsEnabled(MenuBarID::size_custom))
			menu_size->SetLabel(MenuBarID::size_custom, "Custom");
	}
	else if (id == MenuBarID::size_custom)
	{
		wxTextEntryDialog* textEntryDialog = new wxTextEntryDialog(this,
			"The resolution will be 160s x 144s, where s is the scale.", "Enter a scale (positive integer).",
			wxEmptyString, wxTextEntryDialogStyle, wxDefaultPosition);
		textEntryDialog->SetTextValidator(wxFILTER_DIGITS);

		int buttonPressed = textEntryDialog->ShowModal();
		wxString input = textEntryDialog->GetValue();
		textEntryDialog->Destroy();

		if (buttonPressed == wxID_CANCEL)
		{
			// at this time, the 'custom' option has been checked in the menubar
			// calling the below function restores the menu to what is was before the event was propogated
			ApplyGUISettings();
			return;
		}

		scale = wxAtoi(input); // string -> int
		if (scale <= 0) // input converted to int
		{
			wxMessageBox("Please enter a valid scale value (> 0).");
			ApplyGUISettings();
			return;
		}
		menu_size->SetLabel(MenuBarID::size_custom, FormatCustomSizeMenubarLabel(scale));
	}
	else if (id == MenuBarID::size_maximized)
	{
		if (this->IsMaximized())
			this->Restore();
		else
			this->Maximize(true);
		return;
	}
	else // fullscreen
	{
		ToggleFullScreen();
		return;
	}

	emulator.SetWindowScale(scale);
	config.Save();

	if (emulator.emu_is_running)
		SetClientSize(emulator.GetWindowSize());
}


void MainWindow::OnMenuSpeed(wxCommandEvent& event)
{
	// Reset the label of the 'custom' speed option
	if (menu_speed->IsEnabled(MenuBarID::speed_custom))
		menu_speed->SetLabel(MenuBarID::speed_custom, "Custom");

	int id = event.GetId();
	int speed = GetSpeedFromMenuBarID(id);

	if (speed != PRESPECIFIED_SPEED_NOT_FOUND)
	{
		emulator.SetEmulationSpeed(speed);
		emulator.emulation_speed_uncapped = false;
	}
	else if (id == MenuBarID::speed_uncapped)
	{
		emulator.emulation_speed_uncapped = true;
	}
	else // custom speed
	{
		wxTextEntryDialog* textEntryDialog = new wxTextEntryDialog(this,
			"This will be the emulation speed in %.", "Enter a positive integer.",
			wxEmptyString, wxTextEntryDialogStyle, wxDefaultPosition);
		textEntryDialog->SetTextValidator(wxFILTER_DIGITS);

		int buttonPressed = textEntryDialog->ShowModal();
		wxString input = textEntryDialog->GetValue();
		textEntryDialog->Destroy();

		if (buttonPressed == wxID_CANCEL)
		{
			// at this time, the 'custom' option has been checked in the menubar
			// calling the below function restores the menu to what is was before the event was propogated
			ApplyGUISettings();
			return;
		}

		speed = wxAtoi(input);
		if (speed <= 0)
		{
			wxMessageBox("Please enter a valid speed value (> 0).");
			ApplyGUISettings();
			return;
		}
		emulator.SetEmulationSpeed(speed);
		emulator.emulation_speed_uncapped = false;
		menu_speed->SetLabel(MenuBarID::speed_custom, FormatCustomSpeedMenubarLabel(speed));
	}

	config.Save();
}


void MainWindow::OnMenuInput(wxCommandEvent& event)
{
	if (!input_window_active)
		input_bindings_window = new InputBindingsWindow(this, &config, &emulator.joypad, &input_window_active);
	input_bindings_window->Show();
}


void MainWindow::OnMenuToggleFilterFiles(wxCommandEvent& event)
{
	SetupGameList();
	display_only_nes_files = menu_settings->IsChecked(MenuBarID::toggle_filter_nes_files);;
	config.Save();
}


void MainWindow::OnMenuResetSettings(wxCommandEvent& event)
{
	config.SetDefaults();
	ApplyGUISettings();
	if (emulator.emu_is_running)
		SetClientSize(emulator.GetWindowSize());
}


void MainWindow::OnMenuGitHubLink(wxCommandEvent& event)
{
	wxLaunchDefaultBrowser("https://github.com/TesseracT33/nes-dono", wxBROWSER_NEW_WINDOW);
}


void MainWindow::OnListBoxGameSelection(wxCommandEvent& event)
{
	LocateAndOpenRomFromListBoxSelection(event.GetString());
}


void MainWindow::LocateAndOpenRomFromListBoxSelection(wxString selection)
{
	if (rom_folder_path_arr.GetCount() == 0) // when the only item in the listbox is the 'empty_listbox_item'
	{
		ChooseGameDirDialog();
		return;
	}

	// from the selection (rom name; name.ext), get the full path of the file from array `romFolderGamePaths'
	for (const wxString& rom_path : rom_folder_path_arr)
	{
		if (wxFileNameFromPath(rom_path).IsSameAs(selection))
		{
			active_rom_path = rom_path;
			LaunchGame();
			return;
		}
	}

	wxMessageBox(wxString::Format("Error opening game; could not locate file \"%s\".", selection));
}


void MainWindow::OnWindowSizeChanged(wxSizeEvent& event)
{
	UpdateViewSizing();
}


void MainWindow::OnClose(wxCloseEvent& event)
{
	QuitGame();
	SDL_Quit();
	Destroy();
}


void MainWindow::OnKeyDown(wxKeyEvent& event)
{
	int keycode = event.GetKeyCode();

	// key presses are handled differently depending on if we are in the game list box selection view or playing a game
	if (game_list_box->HasFocus())
	{
		switch (keycode)
		{
		case WXK_RETURN:
		{
			wxString selection = game_list_box->GetStringSelection();
			if (selection != wxEmptyString)
				LocateAndOpenRomFromListBoxSelection(selection);
			break;
		}

		case WXK_DOWN:
		{
			int selection = game_list_box->GetSelection();
			game_list_box->SetSelection(std::min(selection + 1, (int)game_list_box->GetCount()));
			break;
		}

		case WXK_UP:
		{
			int selection = game_list_box->GetSelection();
			game_list_box->SetSelection(std::max(selection - 1, 0));
			break;
		}

		default: break;
		}
	}

	else if (emulator.emu_is_running)
	{
		switch (keycode)
		{
		case wx_keycode_save_state:
			emulator.SaveState();
			break;

		case wx_keycode_load_state:
			emulator.LoadState();
			break;

		case wx_keycode_fullscreen:
			ToggleFullScreen();
			break;

		default: break;
		}
	}
}


void MainWindow::OnJoyDown(wxJoystickEvent& event)
{
	event.Skip();
}


void MainWindow::ToggleFullScreen()
{
	if (!emulator.emu_is_running) return;

	this->ShowFullScreen(!full_screen_active, wxFULLSCREEN_ALL);
	full_screen_active = !full_screen_active;
}


// called when the window is resized by the user, to update the sizes of all UI elements
void MainWindow::UpdateViewSizing()
{
	wxSize size = GetClientSize();
	int width = size.GetWidth();
	int height = size.GetHeight();

	if (game_view_active)
	{
		SDL_window_panel->SetSize(size);
		emulator.SetWindowSize(size);
	}
	else
	{
		// The reason for the below nullptr check is that this function will get called automatically by wxwidgets before some child elements have been initialized
		// However, they will be initialized if game_view_active == true
		if (game_list_box != nullptr)
			game_list_box->SetSize(size);
	}
}


void MainWindow::UpdateFPSLabel()
{
	UpdateWindowLabel(true);
	frames_since_update = 0;
}


int MainWindow::GetIdOfSizeMenubarItem(int scale) const
{
	wxString item_string = FormatSizeMenubarLabel(scale);
	int menu_id = menu_size->FindItem(item_string);
	return menu_id;
}


int MainWindow::GetIdOfSpeedMenubarItem(int speed) const
{
	wxString item_string = FormatSpeedMenubarLabel(speed);
	int menu_id = menu_speed->FindItem(item_string);
	return menu_id;
}


wxString MainWindow::FormatSizeMenubarLabel(int scale) const
{
	// format is '160x144 (1x)'
	int width = scale * 160, height = scale * 144;
	return wxString::Format("%ix%i (%ix)", width, height, scale);
}


wxString MainWindow::FormatSpeedMenubarLabel(int speed) const
{
	// format is '100 %'
	return wxString::Format("%i %%", speed);
}


wxString MainWindow::FormatCustomSizeMenubarLabel(int scale) const
{
	// format is 'Custom (1x)'
	return wxString::Format("Custom (%ix)", scale);
}


wxString MainWindow::FormatCustomSpeedMenubarLabel(int speed) const
{
	// format is 'Custom (100 %)'
	return wxString::Format("Custom (%i %%)", speed);
}


int MainWindow::GetSizeFromMenuBarID(int id) const
{
	switch (id)
	{
	case MenuBarID::size_1x: return 1;
	case MenuBarID::size_2x: return 2;
	case MenuBarID::size_4x: return 4;
	case MenuBarID::size_6x: return 6;
	case MenuBarID::size_8x: return 8;
	case MenuBarID::size_10x: return 10;
	case MenuBarID::size_12x: return 12;
	case MenuBarID::size_15x: return 15;
	default: return PRESPECIFIED_SIZE_NOT_FOUND;
	}
}


int MainWindow::GetSpeedFromMenuBarID(int id) const
{
	switch (id)
	{
	case MenuBarID::speed_50:  return 50;
	case MenuBarID::speed_75:  return 75;
	case MenuBarID::speed_100: return 100;
	case MenuBarID::speed_125: return 125;
	case MenuBarID::speed_150: return 150;
	case MenuBarID::speed_200: return 200;
	case MenuBarID::speed_300: return 300;
	case MenuBarID::speed_500: return 500;
	default: return PRESPECIFIED_SPEED_NOT_FOUND;
	}
}


void MainWindow::Configure(Serialization::BaseFunctor& functor)
{
	std::string str = rom_folder_path.ToStdString();
	Serialization::STD_string(functor, str);
	rom_folder_path = wxString(str);

	functor.fun(&display_only_nes_files, sizeof(bool));
}


void MainWindow::SetDefaultConfig()
{
	rom_folder_path = default_rom_folder_path;
	display_only_nes_files = default_display_only_nes_files;
}