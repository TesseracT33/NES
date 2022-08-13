export module BaseMapper;

import MapperProperties;

import NumericalTypes;
import SerializationStream;

import <array>;
import <vector>;

export class BaseMapper
{
public:
	BaseMapper(std::vector<u8> chr_prg_rom, MapperProperties properties);

	/* This function should always be called from the derived classes' 'StreamState' functions. */
	virtual void StreamState(SerializationStream& stream);

	virtual void ClockIRQ() {};
	virtual u8 ReadPRG(u16 addr) = 0;
	virtual u8 ReadCHR(u16 addr) = 0;
	virtual void WritePRG(u16 addr, u8 data) {};
	virtual void WriteCHR(u16 addr, u8 data) {};

	u8 ReadNametableRAM(u16 addr);
	void ReadPRGRAMFromDisk();
	void WriteNametableRAM(u16 addr, u8 data);
	void WritePRGRAMToDisk() const;

protected:
	static void SetCHRBankSize(MapperProperties& properties, std::size_t size);
	static void SetCHRRAMSize(MapperProperties& properties, std::size_t size);
	static void SetPRGRAMSize(MapperProperties& properties, std::size_t size);
	static void SetPRGRAMBankSize(MapperProperties& properties, std::size_t size);
	static void SetPRGROMBankSize(MapperProperties& properties, std::size_t size);

	virtual const std::array<int, 4>& GetNametableMap() const;

	/* https://wiki.nesdev.org/w/index.php/Mirroring */
	static constexpr std::array nametable_map_horizontal = { 0, 0, 1, 1 };
	static constexpr std::array nametable_map_vertical = { 0, 1, 0, 1 };
	static constexpr std::array nametable_map_singlescreen_bottom = { 0, 0, 0, 0 };
	static constexpr std::array nametable_map_singlescreen_top = { 1, 1, 1, 1 };
	static constexpr std::array nametable_map_fourscreen = { 1, 2, 3, 4 };
	static constexpr std::array nametable_map_diagonal = { 1, 2, 2, 1 };

	MapperProperties properties;

	std::vector<u8> chr; /* Either RAM or ROM (a cart cannot have both). */
	std::vector<u8> prg_ram;
	std::vector<u8> prg_rom;

private:
	int GetNametablePage(u16 addr) const;

	std::array<std::array<u8, 0x400>, 4> nametable_ram{};
};