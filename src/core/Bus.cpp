#include "Bus.h"


void Bus::Initialize()
{

}


void Bus::Reset()
{

}


void Bus::Serialize(std::ofstream& ofs)
{

}


void Bus::Deserialize(std::ifstream& ifs)
{

}


u8 Bus::Read(u16 addr)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		return memory.ram[addr & 0x7FF]; // wrap address to between 0-0x7FF
	}

	// PPU Registers($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		return memory.ppu_regs[(addr - 0x2000) & 7]; // wrap address to between 0x2000-0x2007 
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		return memory.apu_io_regs[addr - 0x4000];
	}

	// APU Test Registers ($4018 - $401F)
	else if (addr <= 0x401F) [[unlikely]]
	{
		//unused
	}

	// Cartridge Space($4020 - $FFFF)
	else
	{
		return cartridge->Read(addr);
	}
}


void Bus::Write(u16 addr, u8 data)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		memory.ram[addr & 0x7FF] = data; // wrap address to between 0-0x7FF
	}

	// PPU Registers($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		memory.ppu_regs[(addr - 0x2000) & 7] = data; // wrap address to between 0x2000-0x2007 
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		memory.apu_io_regs[addr - 0x4000] = data;
	}

	// APU Test Registers ($4018 - $401F)
	else if (addr <= 0x401F) [[unlikely]]
	{
		//unused
	}

	// Cartridge Space($4020 - $FFFF)
	else
	{
		cartridge->Write(addr, data);
	}
}
