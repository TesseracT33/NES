#pragma once

#include "wx/wx.h"

#include "MainWindow.h"

class App : public wxApp
{
public:
	App();
	~App();

	bool OnInit() override;

private:
	MainWindow* main_window = nullptr;
};