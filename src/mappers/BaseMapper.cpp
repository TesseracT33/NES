module BaseMapper;

BaseMapper::BaseMapper(std::vector<u8> chr_prg_rom, MapperProperties properties) : properties(properties)
{
	/* These must be calculated here, and cannot be part of the properties passed to the submapper constructor,
	   as bank sizes are not known before the submapper constructors have been called. */
	this->properties.num_chr_banks = properties.chr_size / properties.chr_bank_size;
	this->properties.num_prg_ram_banks = properties.prg_ram_size / properties.prg_ram_bank_size;
	this->properties.num_prg_rom_banks = properties.prg_rom_size / properties.prg_rom_bank_size;

	/* Resize all vectors */
	chr.resize(properties.chr_size);
	prg_ram.resize(properties.prg_ram_size);
	prg_rom.resize(properties.prg_rom_size);

	/* Fill vectors with either rom data or $00 */
	std::copy(chr_prg_rom.begin(), chr_prg_rom.begin() + properties.prg_rom_size, prg_rom.begin());

	if (!properties.has_chr_ram) {
		std::copy(chr_prg_rom.begin() + properties.prg_rom_size, chr_prg_rom.end(), chr.begin());
	}
	else {
		std::fill(chr.begin(), chr.end(), 0x00);
	}
	if (!prg_ram.empty()) {
		std::fill(prg_ram.begin(), prg_ram.end(), 0x00);
	}
	for (auto& nametable_arr : nametable_ram) {
		nametable_arr.fill(0x00);
	}
}


void BaseMapper::ReadPRGRAMFromDisk()
{
	//if (properties.has_persistent_prg_ram) {
	//	const std::string save_data_path = properties.rom_path + save_file_postfix;
	//	if (AppUtils::FileExists(save_data_path)) {
	//		std::ifstream ifs{ save_data_path, std::ifstream::in | std::ofstream::binary };
	//		if (!ifs) {
	//			UserMessage::Show("Save file loading failed!");
	//			return;
	//		}
	//		ifs.read((char*)prg_ram.data(), prg_ram.size());
	//	}
	//}
}


void BaseMapper::WritePRGRAMToDisk() const
{
	//if (properties.has_persistent_prg_ram)
	//{
	//	static bool save_data_creation_has_failed = false;
	//	if (!save_data_creation_has_failed) /* Avoid the spamming of user messages, since this function is called regularly. */
	//	{
	//		const std::string save_data_path = properties.rom_path + save_file_postfix;
	//		std::ofstream ofs{ save_data_path, std::ofstream::out | std::ofstream::binary };
	//		if (!ofs)
	//		{
	//			save_data_creation_has_failed = true;
	//			UserMessage::Show("Save file creation failed!");
	//			return;
	//		}
	//		ofs.write((const char*)prg_ram.data(), prg_ram.size());
	//	}
	//}
}


u8 BaseMapper::ReadNametableRAM(u16 addr)
{
	int page = GetNametablePage(addr);
	return nametable_ram[page][addr & 0x3FF];
}


void BaseMapper::WriteNametableRAM(u16 addr, u8 data)
{
	int page = GetNametablePage(addr);
	nametable_ram[page][addr & 0x3FF] = data;
}


const std::array<int, 4>& BaseMapper::GetNametableMap() const
{
	if (properties.mirroring == 0) {
		return nametable_map_horizontal;
	}
	else {
		return nametable_map_vertical;
	}
}


/* The following static functions may be called from submapper constructors.
	The submapper classes must apply these properties themselves; they cannot be deduced from the rom header. */
void BaseMapper::SetCHRBankSize(MapperProperties& properties, std::size_t size)
{
	properties.chr_bank_size = size;
}


void BaseMapper::SetPRGRAMBankSize(MapperProperties& properties, std::size_t size)
{
	properties.prg_ram_bank_size = size;
}


void BaseMapper::SetPRGROMBankSize(MapperProperties& properties, std::size_t size)
{
	properties.prg_rom_bank_size = size;
}


/* A submapper constructor must call this function if it has CHR RAM, because if it has RAM instead of ROM,
	the CHR size specified in the rom header will always (?) be 0. */
void BaseMapper::SetCHRRAMSize(MapperProperties& properties, std::size_t size)
{
	if (properties.has_chr_ram && properties.chr_size == 0) {
		properties.chr_size = size;
	}
}


/* The PRG RAM size (or PRG RAM presence) may or may not be specified in the rom header,
	in particular if using iNES and not NES 2.0.
	For now, let games with mappers that support PRG RAM, always have PRG RAM of some predefined size. */
void BaseMapper::SetPRGRAMSize(MapperProperties& properties, std::size_t size)
{
	if (properties.prg_ram_size == 0) {
		properties.has_prg_ram = true;
		properties.prg_ram_size = size;
	}
}


int BaseMapper::GetNametablePage(u16 addr) const
{
	const std::array<int, 4>& map = GetNametableMap();
	int quadrant = (addr & 0xF00) >> 10; /* $2000-$23FF ==> 0; $2400-$27FF ==> 1; $2800-$2BFF ==> 2; $2C00-$2FFF ==> 3 */
	int page = map[quadrant];
	return page;
}


void BaseMapper::StreamState(SerializationStream& stream)
{
	stream.StreamArray(nametable_ram);
	stream.StreamVector(prg_ram);
	if (properties.has_chr_ram) {
		stream.StreamVector(chr);
	}
}