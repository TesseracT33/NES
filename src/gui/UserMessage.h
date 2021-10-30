#pragma once

#include <string>

#include "wx/wx.h"

namespace UserMessage
{
	enum class Type { Unspecified, Success, Warning, Error, Fatal };

	inline void Show(const wxString& message, Type type = Type::Unspecified)
	{
		wxString prefix{};
		switch (type)
		{
		case Type::Success: prefix = "Success: "; break;
		case Type::Warning: prefix = "Warning: "; break;
		case Type::Error: prefix = "Error: "; break;
		case Type::Fatal: prefix = "Fatal: "; break;
		default: break;
		}
		wxMessageBox(prefix + message);
	}

	inline void Show(const std::string& message, Type type = Type::Unspecified)
	{
		Show(wxString(message), type);
	}

	inline void Show(const char* message, Type type = Type::Unspecified)
	{
		Show(wxString(message), type);
	}
}