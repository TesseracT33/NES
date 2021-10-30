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
	std::ofstream ofs(config_file_path.mb_str(), std::ofstream::out | std::ofstream::binary);
	if (!ofs) // if the file could not be created
	{
		UserMessage::Show("Config file could not be created or saved.", UserMessage::Type::Warning);
		SetDefaults(false);
		return;
	}

	Serialization::SerializeFunctor functor{ ofs };

	for (Configurable* configurable : configurables)
		configurable->Configure(functor);

	ofs.close();
}


void Config::Load()
{
	std::ifstream ifs(config_file_path.mb_str(), std::ifstream::in | std::ifstream::binary);
	if (!ifs) // if the file could not be opened
	{
		UserMessage::Show("Config file could not be opened. Reverting to defaults.", UserMessage::Type::Warning);
		SetDefaults(false);
		return;
	}

	Serialization::DeserializeFunctor functor{ ifs };

	for (Configurable* configurable : configurables)
		configurable->Configure(functor);

	ifs.close();
}


void Config::SetDefaults(bool save)
{
	for (Configurable* configurable : configurables)
		configurable->SetDefaultConfig();

	if (save)
		Save();
}