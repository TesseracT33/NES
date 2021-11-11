#include "Config.h"


void Config::AddConfigurable(Configurable* configurable)
{
	assert(configurable != nullptr);
	this->configurables.push_back(configurable);
}


bool Config::ConfigFileExists()
{
	return AppUtils::FileExists(config_file_path);
}


void Config::Save()
{
	SerializationStream stream{ config_file_path, SerializationStream::Mode::Serialization };
	if (stream.error)
	{
		UserMessage::Show("Config file could not be created or saved.", UserMessage::Type::Error);
		SetDefaults(false);
		return;
	}

	for (Configurable* configurable : configurables)
		configurable->StreamConfig(stream);
}


void Config::Load()
{
	SerializationStream stream{ config_file_path, SerializationStream::Mode::Deserialization };
	if (stream.error)
	{
		UserMessage::Show("Config file could not be opened. Reverting to defaults.", UserMessage::Type::Error);
		SetDefaults(false);
		return;
	}

	for (Configurable* configurable : configurables)
		configurable->StreamConfig(stream);
}


void Config::SetDefaults(bool save)
{
	for (Configurable* configurable : configurables)
		configurable->SetDefaultConfig();

	if (save)
		Save();
}