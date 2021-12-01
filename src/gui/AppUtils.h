#pragma once

#include <string>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include "wx/wx.h"

namespace AppUtils
{
	inline std::string GetExecutablePath()
	{
		wxString path = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
		return path.ToStdString();
	}

	inline std::string GetFileNameFromPath(const std::string& path)
	{
		return wxFileName(path).GetName().ToStdString();
	}

	inline bool FileExists(const wxString& path)
	{
		return wxFileExists(path);
	}

	inline bool FileExists(const std::string& path)
	{
		return FileExists(wxString(path));
	}

	inline bool FileExists(const char* path)
	{
		return FileExists(wxString(path));
	}
}