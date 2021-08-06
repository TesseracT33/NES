#pragma once

#include <wx/msgdlg.h>

#include "Component.h"
#include "Header.h"

#include "mappers/BaseMapper.h"
#include "mappers/MapperEnum.h"
#include "mappers/NROM.h"

class Cartridge final : public Component
{
public:
	void Initialize() override;
	void Reset() override;

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

	void Eject();
	u8 Read(u16 addr, bool ppu = false) const;
	bool ReadRomFile(const char* path);
	void Write(u16 addr, u8 data, bool ppu = false);

private:
	static const size_t header_size = 0x10;
	static const size_t trainer_size = 0x200;
	static const size_t prg_piece_size = 0x4000;
	static const size_t chr_piece_size = 0x4000;
	
	Header header;

	std::unique_ptr<BaseMapper> mapper;

	void ParseRomHeader(u8* header_arr);
	void ConstructMapper();
	void LayoutMapperMemory(u8* rom_arr);
};

