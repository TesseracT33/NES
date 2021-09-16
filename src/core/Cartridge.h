#pragma once

#include <wx/msgdlg.h>

#include "Component.h"
#include "Header.h"
#include "PPU.h"

#include "mappers/BaseMapper.h"
#include "mappers/MapperIncludes.h"

class Cartridge final : public Component
{
public:
	PPU* ppu;

	void Initialize();
	void Reset();

	void State(Serialization::BaseFunctor& functor) override;

	void Eject();
	u8 Read(u16 addr, bool ppu = false) const;
	bool ReadRomFile(std::string path);
	void Write(u16 addr, u8 data, bool ppu = false);

private:
	static const size_t header_size = 0x10;
	static const size_t trainer_size = 0x200;
	static const size_t prg_piece_size = 0x4000;
	static const size_t chr_piece_size = 0x2000;

	std::shared_ptr<BaseMapper> mapper;

	Header header;

	template<typename Mapper> void MapperFactory() { this->mapper = std::make_shared<Mapper>(); }

	void ParseRomHeader(u8* header_arr);
	void ConstructMapper();
	void LayoutMapperMemory(u8* rom_arr);
};

