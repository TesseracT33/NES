#pragma once

#include "Configurable.h"

#include "gui/AppUtils.h"
#include "gui/UserMessage.h"

class Config
{
public:
	std::vector<Configurable*> configurables;

	void AddConfigurable(Configurable* configurable);

	bool ConfigFileExists();
	void Load();
	void Save();
	void SetDefaults(bool save = true);

private:
	const std::string config_file_path = AppUtils::GetExecutablePath() + "config.bin";
};