#include "App.h"

wxIMPLEMENT_APP(App);


App::App()
{
}


App::~App()
{
}


bool App::OnInit()
{
	main_window = new MainWindow();
	main_window->Show();
	return true;
}