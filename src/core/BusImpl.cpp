#include "BusImpl.h"


void BusImpl::Initialize()
{

}


void BusImpl::Reset()
{

}


u8 BusImpl::Read(u16 addr)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		return memory.ram[addr & 0x7FF]; // wrap address to between 0-0x7FF
	}

	// PPU Registers ($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		// Wrap address to between 0x2000-0x2007 
		return ppu->ReadRegister(0x2000 + (addr & 7));
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		switch (addr)
		{
		case Bus::Addr::JOY1: 
		case Bus::Addr::JOY2: return joypad->ReadReg(addr);
		default: return memory.apu_io_regs[addr - 0x4000];
		}
	}

	// APU Test Registers ($4018 - $401F)
	else if (addr <= 0x401F) [[unlikely]]
	{
		//unused
	}

	// Cartridge Space ($4020 - $FFFF)
	else
	{
		return cartridge->Read(addr);
	}
}


void BusImpl::Write(u16 addr, u8 data)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		memory.ram[addr & 0x7FF] = data; // wrap address to between 0-0x7FF
	}

	// PPU Registers($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		// wrap address to between 0x2000-0x2007 
		addr = 0x2000 + (addr & 7);
		ppu->WriteRegister(addr, data);
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		switch (addr)
		{
		case Bus::Addr::JOY1: 
		case Bus::Addr::JOY2: joypad->WriteReg(addr, data); break;
		default: memory.apu_io_regs[addr - 0x4000] = data;
		}
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


u8 BusImpl::ReadCycle(u16 addr)
{
	u8 read = Read(addr);
	apu->Update();
	ppu->Update();
	cpu->IncrementCycleCounter();
	return read;
}


void BusImpl::WriteCycle(u16 addr, u8 data)
{
	Write(addr, data);
	apu->Update();
	ppu->Update();
	cpu->IncrementCycleCounter();
}


void BusImpl::WaitCycle(unsigned cycles)
{
	for (unsigned i = 0; i < cycles; i++)
	{
		apu->Update();
		ppu->Update();
		cpu->IncrementCycleCounter();
	}
}


void BusImpl::State(Serialization::BaseFunctor& functor)
{

}
