#include "BusImpl.h"


void BusImpl::Initialize()
{

}


void BusImpl::Reset()
{
	ram.fill(0);
	apu_io_test.fill(0);
}


u8 BusImpl::Read(u16 addr)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		return ram[addr & 0x7FF]; // wrap address to between 0-0x7FF
	}

	// PPU Registers ($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		// Wrap address to between 0x2000-0x2007 
		return ppu->ReadRegister(addr & 0x2007);
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		switch (addr)
		{
		case Bus::Addr::OAMADDR: // $4014
			return ppu->ReadRegister(addr);
		case Bus::Addr::JOY1: // $4016
		case Bus::Addr::JOY2: // $4017
			return joypad->ReadRegister(addr);
		default:
			return apu->ReadRegister(addr);
		}
	}

	// APU Test Registers ($4018 - $401F)
	else if (addr <= 0x401F) [[unlikely]]
	{
		//unused
		return 0xFF;
	}

	// Cartridge Space ($4020 - $FFFF)
	else
	{
		return mapper->ReadPRG(addr);
	}
}


void BusImpl::Write(u16 addr, u8 data)
{
	// Internal RAM ($0000 - $1FFF)
	if (addr <= 0x1FFF)
	{
		ram[addr & 0x7FF] = data; // wrap address to between 0-0x7FF
	}

	// PPU Registers ($2000 - $3FFF)
	else if (addr <= 0x3FFF)
	{
		// wrap address to between 0x2000-0x2007 
		ppu->WriteRegister(addr & 0x2007, data);
	}

	// APU & I/O Registers ($4000-$4017)
	else if (addr <= 0x4017)
	{
		switch (addr)
		{
		case Bus::Addr::OAMDMA: // $4014
			ppu->WriteRegister(addr, data);
			break;
		case Bus::Addr::JOY1: // $4016
			joypad->WriteRegister(addr, data);
			break;
		default:
			apu->WriteRegister(addr, data); /* Note: $4017 is treated as an apu register here, unlike in the read function. */
			break;
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
		mapper->WritePRG(addr, data);
	}
}


u8 BusImpl::ReadCycle(u16 addr)
{
	u8 val = Read(addr);
	apu->Update();
	ppu->Update();
	UpdateLogging();
	return val;
}


void BusImpl::WriteCycle(u16 addr, u8 data)
{
	Write(addr, data);
	apu->Update();
	ppu->Update();
	UpdateLogging();
}


void BusImpl::WaitCycle()
{
	apu->Update();
	ppu->Update();
	UpdateLogging();
}


void BusImpl::State(Serialization::BaseFunctor& functor)
{

}


void BusImpl::UpdateLogging()
{
#ifdef DEBUG
	if (update_logging_on_next_cycle)
	{
		Logging::Update();
		update_logging_on_next_cycle = false;
	}
#endif DEBUG
}
