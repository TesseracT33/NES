#include "CPU.h"


CPU::CPU()
{
	BuildInstrTypeTable();
}


void CPU::BuildInstrTypeTable()
{
	auto GetInstrType = [&](instr_t instr)
	{
		// A switch stmnt is not possible, as 'instr' is not an integral or enum type
		// This function only gets executed at program startup anyways, so the poor performance does not matter
		
		// Note: some unofficial instructions (ALR, ARR, RLA, RRA) are the combination of two instructions, one of which is a read-modify-write instruction. 
		// However, the addressing mode is immediate in these cases, meaning that the result of the r-m-w instruction won't be stored anywhere.
		// The other instruction is a read instruction. Thus, such unofficial instructions can be set to be read instructions.
		// TODO: are these instructions actually the correct length (timing)?
		if (instr == &CPU::ADC || instr == &CPU::AND || instr == &CPU::BIT || instr == &CPU::CMP ||
			instr == &CPU::CPX || instr == &CPU::CPY || instr == &CPU::EOR || instr == &CPU::LDA || 
			instr == &CPU::LDX || instr == &CPU::LDY || instr == &CPU::ORA || instr == &CPU::SBC ||
			instr == &CPU::ALR || instr == &CPU::ANC || instr == &CPU::ARR || instr == &CPU::LAS ||
			instr == &CPU::LAX || instr == &CPU::XAA)
			return InstrType::Read;

		if (instr == &CPU::STA || instr == &CPU::STX || instr == &CPU::STY || instr == &CPU::AHX || 
			instr == &CPU::AXS || instr == &CPU::SAX || instr == &CPU::SHX || instr == &CPU::SHY || 
			instr == &CPU::TAS)
			return InstrType::Write;

		if (instr == &CPU::ASL || instr == &CPU::DEC || instr == &CPU::INC || instr == &CPU::LSR ||
			instr == &CPU::ROL || instr == &CPU::ROR || instr == &CPU::DCP || instr == &CPU::ISC ||
			instr == &CPU::RLA || instr == &CPU::RRA || instr == &CPU::SLO || instr == &CPU::SRE)
			return InstrType::Read_modify_write;

		return InstrType::Implicit;
	};

	for (size_t i = 0; i < num_instr; i++)
	{
		instr_t instr = instr_table[i];
		instr_type_table[i] = GetInstrType(instr);
	}
}


void CPU::Power()
{
	// https://wiki.nesdev.com/w/index.php?title=CPU_power_up_state

	Reset();

	SetStatusReg(0x34);
	A = X = Y = 0;
	S = 0xFD;
	for (u16 addr = 0x4000; addr <= 0x4013; addr++)
		bus->Write(addr, 0);
	bus->Write(0x4015, 0);
	bus->Write(0x4017, 0);
}


void CPU::Reset()
{
	cpu_clocks_since_reset = 0;
	all_ppu_regs_writable = false;
	NMI_signal_active = false;
	IRQ_num_inputs = 0;
	set_I_on_next_update = clear_I_on_next_update = false;
	stopped = false;
	oam_dma_transfer_active = false;

	PC = bus->Read(Bus::Addr::RESET_VEC) | bus->Read(Bus::Addr::RESET_VEC + 1) << 8;
}


// Step one cpu cycle
void CPU::Update()
{
	IncrementCycleCounter();

	if (stopped) return;

	#ifdef DEBUG
		cpu_cycle_counter++;
	#endif

	if (!all_ppu_regs_writable && ++cpu_clocks_since_reset == cpu_clocks_until_all_ppu_regs_writable)
		all_ppu_regs_writable = true;

	if (set_I_on_next_update)
	{
		flags.I = 1;
		set_I_on_next_update = false;
	}
	else if (clear_I_on_next_update)
	{
		flags.I = 0;
		clear_I_on_next_update = false;
	}

	if (oam_dma_transfer_active)
	{
		UpdateOAMDMATransfer();
	}
	// If an instruction is currently being executed
	else if (curr_instr.instr_executing)
	{
		// Continue the execution of the instruction
		std::invoke(curr_instr.addr_mode_fun, this);
	}
	else
	{
		BeginInstruction();

		// Check for pending interrupts (NMI and IRQ); NMI has higher priority than IRQ
		// Interrupts are only polled after executing an instruction; multiple interrupts cannot be serviced in a row
		if (NMI_signal_active)
			ServiceInterrupt(InterruptType::NMI);
		else if (IRQ_num_inputs > 0 && !flags.I)
			ServiceInterrupt(InterruptType::IRQ);
	}
}


void CPU::IncrementCycleCounter(unsigned cycles)
{
#ifdef DEBUG
	cpu_cycle_counter += cycles;
#endif

	if (cycles & 1)
		odd_cpu_cycle = !odd_cpu_cycle;
}


void CPU::StartOAMDMATransfer(u8 page, u8* oam_start_ptr)
{
	oam_dma_transfer_active = true;
	oam_dma_base_addr = page << 8;
	this->oam_start_ptr = oam_start_ptr;
	oam_dma_bytes_copied = 0;

	// Before the DMA transfer starts, there is/are one or two wait cycles, depending on if the current cpu cycle is even or odd
	bus->WaitCycle(odd_cpu_cycle ? 2 : 1);
}


void CPU::UpdateOAMDMATransfer()
{
	*(oam_start_ptr + oam_dma_bytes_copied) = bus->ReadCycle(oam_dma_base_addr + oam_dma_bytes_copied);
	if (++oam_dma_bytes_copied == 0x100)
		oam_dma_transfer_active = false;
}


void CPU::BeginInstruction()
{
	curr_instr.opcode = bus->Read(PC++);
	curr_instr.addr_mode = GetAddressingModeFromOpcode(curr_instr.opcode);
	curr_instr.addr_mode_fun = addr_mode_fun_table[static_cast<int>(curr_instr.addr_mode)];
	curr_instr.instr = instr_table[curr_instr.opcode];
	curr_instr.instr_type = instr_type_table[curr_instr.opcode];
	curr_instr.instr_executing = true;
	curr_instr.cycle = 1;
}


void CPU::StepImplicit()
{
	// Note: some instructions using implicit addressing take longer than two cycles. This is handled in the functions for those themselves
	std::invoke(curr_instr.instr, this);
	curr_instr.instr_executing = false;
}


void CPU::StepAccumulator()
{
	// all instructions using the accumulator addressing mode take two cycles (first one is from when fetching the opcode)
	curr_instr.read_addr = A;
	std::invoke(curr_instr.instr, this);
	A = curr_instr.new_target;
	curr_instr.instr_executing = false;
}


void CPU::StepImmediate()
{
	// all instructions using the immediate addressing mode take two cycles (first one is from when fetching the opcode)
	curr_instr.read_addr = bus->Read(PC++);
	std::invoke(curr_instr.instr, this);
	curr_instr.instr_executing = false;
}


void CPU::StepZeroPage()
{
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr = curr_instr.addr_lo;
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 3:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 4:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepZeroPageIndexed(u8& index_reg)
{
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr = curr_instr.addr_lo;
		bus->Read(curr_instr.addr);
		curr_instr.addr = (curr_instr.addr + index_reg) & 0xFF;
		return;

	case 3:
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 4:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 5:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepAbsolute()
{
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr_hi = bus->Read(PC++);
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo;
		// JSR, JMP instructions differently than other instructions with absolute addressing; they don't do the same reads and writes as others
		if (curr_instr.opcode == 0x20 || curr_instr.opcode == 0x4C)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 3:
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 4:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 5:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepAbsoluteIndexed(u8& index_reg)
{
	static bool addition_overflow;

	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr_hi = bus->Read(PC++);
		addition_overflow = curr_instr.addr_lo + index_reg > 0xFF;
		curr_instr.addr_lo = curr_instr.addr_lo + index_reg;
		return;

	case 3:
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo;
		curr_instr.read_addr = bus->Read(curr_instr.addr);

		if (addition_overflow)
			curr_instr.addr = ((curr_instr.addr_hi + 1) & 0xFF) << 8 | curr_instr.addr_lo;
		// According to nestest, NOPs with absolute indexed addressing should be 4 cycles long
		else if (curr_instr.instr_type == InstrType::Read || curr_instr.instr_type == InstrType::Implicit)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 4:
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 5:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 6:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepRelative()
{
	// TODO: unsure about timing
	// Branch instructions (only these use relative addressing) take 2 + 1 or 2 + 2 cycles (+1 if branch succeeds, +2 if to a new page)
	// However, all branching logic is inside of the Branch() function, including additional wait cycles

	curr_instr.addr_lo = bus->Read(PC++);
	std::invoke(curr_instr.instr, this);
	curr_instr.instr_executing = false;
}


void CPU::StepIndirect()
{
	static u16 addr_tmp = 0;

	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr_hi = bus->Read(PC++);
		return;

	case 3:
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo;
		addr_tmp = bus->Read(curr_instr.addr);
		return;

	case 4:
		// HW bug: if 'curr_instr.addr' is xyFF, then the upper byte of 'addr_tmp' is fetched from xy00, not (xy+1)00
		if (curr_instr.addr_lo == 0xFF)
			addr_tmp |= bus->Read(curr_instr.addr_hi << 8) << 8;
		else
			addr_tmp |= bus->Read(curr_instr.addr + 1) << 8;
		curr_instr.addr = addr_tmp;
		std::invoke(curr_instr.instr, this);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepIndexedIndirect()
{
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr = curr_instr.addr_lo;
		bus->Read(curr_instr.addr);
		curr_instr.addr = (curr_instr.addr + X) & 0xFF;
		return;

	case 3:
		curr_instr.read_addr = bus->Read(curr_instr.addr);
		curr_instr.addr = (curr_instr.addr + 1) & 0xFF;
		return;

	case 4:
		curr_instr.addr = bus->Read(curr_instr.addr) << 8 | curr_instr.read_addr;
		return;

	case 5:
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 6:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 7:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


void CPU::StepIndirectIndexed()
{
	static bool addition_overflow;

	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.read_addr = bus->Read(curr_instr.addr_lo);
		curr_instr.addr_lo++;
		return;

	case 3:
		curr_instr.addr_hi = bus->Read(curr_instr.addr_lo);
		addition_overflow = curr_instr.read_addr + Y > 0xFF;
		curr_instr.addr_lo = curr_instr.read_addr + Y;
		return;

	case 4:
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo; // todo: not 100 % sure if correct
		curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (addition_overflow)
			curr_instr.addr = ((curr_instr.addr_hi + 1) & 0xFF) << 8 | curr_instr.addr_lo;
		else if (curr_instr.instr_type == InstrType::Read)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 5:
		if (curr_instr.instr_type != InstrType::Write)
			curr_instr.read_addr = bus->Read(curr_instr.addr);
		if (curr_instr.instr_type != InstrType::Read_modify_write)
		{
			std::invoke(curr_instr.instr, this);
			curr_instr.instr_executing = false;
		}
		return;

	case 6:
		bus->Write(curr_instr.addr, curr_instr.read_addr);
		std::invoke(curr_instr.instr, this);
		return;

	case 7:
		bus->Write(curr_instr.addr, curr_instr.new_target);
		curr_instr.instr_executing = false;
	}
}


CPU::AddrMode CPU::GetAddressingModeFromOpcode(u8 opcode) const
{
	switch (opcode & 0x1F)
	{
	case 0x00:
		if (opcode == 0x20)           return AddrMode::Absolute;
		if ((opcode & ~0x1F) >= 0x80) return AddrMode::Immediate;
		return AddrMode::Implicit;
	case 0x01: return AddrMode::Indexed_indirect;
	case 0x02: return (opcode & ~0x1F) >= 0x80 ? AddrMode::Immediate : AddrMode::Implicit;
	case 0x03: return AddrMode::Indexed_indirect;
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: return AddrMode::Zero_page;
	case 0x08: return AddrMode::Implicit;
	case 0x09: return AddrMode::Immediate;
	case 0x0A: return (opcode & ~0x1F) >= 0x80 ? AddrMode::Implicit : AddrMode::Accumulator;
	case 0x0B: return AddrMode::Immediate;
	case 0x0C: return (opcode == 0x6C) ? AddrMode::Indirect : AddrMode::Absolute;
	case 0x0D:
	case 0x0E:
	case 0x0F: return AddrMode::Absolute;
	case 0x10: return AddrMode::Relative;
	case 0x11: return AddrMode::Indirect_indexed;
	case 0x12: return AddrMode::Implicit;
	case 0x13: return AddrMode::Indirect_indexed;
	case 0x14:
	case 0x15: return AddrMode::Zero_page_X;
	case 0x16:
	case 0x17: return (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Zero_page_Y : AddrMode::Zero_page_X;
	case 0x18: return AddrMode::Implicit;
	case 0x19: return AddrMode::Absolute_Y;
	case 0x1A: return AddrMode::Implicit;
	case 0x1B: return AddrMode::Absolute_Y;
	case 0x1C:
	case 0x1D: return AddrMode::Absolute_X;
	case 0x1E:
	case 0x1F: return (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Absolute_Y : AddrMode::Absolute_X;
	}
}


void CPU::SetIRQLow()
{
	// todo: probably needs some code that prevents this var from being increment twice by the same component?
	IRQ_num_inputs++;
}


void CPU::SetIRQHigh()
{
	IRQ_num_inputs--;
}


void CPU::SetNMILow()
{
	NMI_signal_active = true;
}


void CPU::SetNMIHigh()
{
	NMI_signal_active = false;
}


void CPU::ServiceInterrupt(InterruptType asserted_interrupt_type)
{
	/* IRQ and NMI tick-by-tick execution
	  #  address R/W description
	 --- ------- --- -----------------------------------------------
	  1    PC     R  fetch opcode (and discard it - $00 (BRK) is forced into the opcode register instead)
	  2    PC     R  read next instruction byte (actually the same as above, since PC increment is suppressed. Also discarded.)
	  3  $0100,S  W  push PCH on stack, decrement S
	  4  $0100,S  W  push PCL on stack, decrement S
	 *** At this point, the signal status determines which interrupt vector is used ***
	  5  $0100,S  W  push P on stack (with B flag *clear*), decrement S
	  6   A       R  fetch PCL (A = FFFE for IRQ, A = FFFA for NMI), set I flag
	  7   A       R  fetch PCH (A = FFFF for IRQ, A = FFFB for NMI)

	 For BRK: the first two cycles differ as follows:
	  1    PC     R  fetch opcode, increment PC
	  2    PC     R  read next instruction byte (and throw it away), increment PC
	*/

	// Cycles 1-2
	if (asserted_interrupt_type == InterruptType::BRK)
	{
		// note: the first read and increment of PC has already been handled in the BRK() function
		bus->ReadCycle(PC++);
	}
	else
	{
		bus->ReadCycle(PC);
		bus->ReadCycle(PC);
	}

	// Cycles 3-4
	bus->WriteCycle(0x0100 | S--, PC >> 8);
	bus->WriteCycle(0x0100 | S--, PC & 0xFF);

	// Interrupt hijacking: If both an NMI and an IRQ are pending, the NMI will be handled and the pending status of the IRQ forgotten
	// The same applies to IRQ and BRK; an IRQ can hijack a BRK
	if (asserted_interrupt_type == InterruptType::NMI || NMI_signal_active)
	{
		handled_interrupt_type = InterruptType::NMI;
	}
	else
	{
		if (asserted_interrupt_type == InterruptType::IRQ || IRQ_num_inputs > 0 && !flags.I)
			handled_interrupt_type = InterruptType::IRQ;
		else
			handled_interrupt_type = InterruptType::BRK;
	}

	// cycle 5
	bus->WriteCycle(0x0100 | S--, GetStatusRegInterrupt());

	// cycles 6-7
	if (handled_interrupt_type == InterruptType::NMI)
	{
		PC = PC & 0xFF00 | bus->ReadCycle(Bus::Addr::NMI_VEC);
		PC = PC & 0x00FF | bus->ReadCycle(Bus::Addr::NMI_VEC + 1) << 8;
		NMI_signal_active = false; // todo: it's not entirely clear if this is the right place for it
	}
	else
	{
		PC = PC & 0xFF00 | bus->ReadCycle(Bus::Addr::IRQ_BRK_VEC);
		PC = PC & 0x00FF | bus->ReadCycle(Bus::Addr::IRQ_BRK_VEC + 1) << 8;
	}
	flags.I = 1; // it's OK if this is set in cycle 7 instead of 6; the flag isn't used until after this function returns

	if (handled_interrupt_type == InterruptType::BRK)
	{
		// If the interrupt servicing is forced from the BRK instruction, the B flag is also set
		// According to http://obelisk.me.uk/6502/reference.html#BRK, the flag should be set after pushing the processor status on the stack, and not before
		flags.B = 1;
	}
}


// Add the contents of a memory location to the accumulator together with the carry bit. If overflow occurs the carry bit is set.
void CPU::ADC()
{
	u8 op = curr_instr.read_addr + flags.C;
	flags.V = ((A & 0x7F) + (curr_instr.read_addr & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + (curr_instr.read_addr       ) + flags.C > 0xFF);
	flags.C = A + op > 0xFF;
	A += op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Bitwise AND between the accumulator and the contents of a memory location.
void CPU::AND()
{
	u8 op = curr_instr.read_addr;
	A &= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Shift all bits of the accumulator or the contents of a memory location one bit left. Bit 0 is cleared and bit 7 is placed in the carry flag.
void CPU::ASL()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target << 1 & 0xFF;
	curr_instr.new_target = new_target;
	flags.C = target & 0x80;
	flags.Z = new_target == 0;
	flags.N = new_target & 0x80;
}


// If the carry flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BCC()
{
	Branch(!flags.C);
}


// If the carry flag is set then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BCS()
{
	Branch(flags.C);
}


// If the zero flag is set then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BEQ()
{
	Branch(flags.Z);
}


// Check the bitwise AND between the accumulator and the contents of a memory location, and set the status flags accordingly.
void CPU::BIT()
{
	u8 op = curr_instr.read_addr;
	flags.Z = (A & op) == 0;
	flags.V = op & 0x40;
	flags.N = op & 0x80;
}


// If the negative flag is set then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BMI()
{
	Branch(flags.N);
}


// If the zero flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BNE()
{
	Branch(!flags.Z);
}


// If the negative flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BPL()
{
	Branch(!flags.N);
}


// Force the generation of an interrupt request. Additionally, the break flag is set.
void CPU::BRK()
{
	ServiceInterrupt(InterruptType::BRK);
}


// If the overflow flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BVC()
{
	Branch(!flags.V);
}


// If the overflow flag is set then add the relative displacement to the program counter to cause a branch to a new location.
void CPU::BVS()
{
	Branch(flags.V);
}


// Clear the carry flag.
void CPU::CLC()
{
	flags.C = 0;
}


// Clear the decimal mode flag.
void CPU::CLD()
{
	flags.D = 0;
}


// Clear the interrupt disable flag.
void CPU::CLI()
{
	// CLI clears the I flag after polling for interrupts (effectively when CPU::Update() is called next time)
	clear_I_on_next_update = true;
}


// Clear the overflow flag.
void CPU::CLV()
{
	flags.V = 0;
}


// Compare the contents of the accumulator with the contents of a memory location (essentially performing the subtraction A-M without storing the result).
void CPU::CMP()
{
	u8 M = curr_instr.read_addr;
	flags.C = A >= M;
	flags.Z = A == M;
	u8 result = A - M;
	flags.N = result & 0x80;
}


// Compare the contents of the X register with the contents of a memory location (essentially performing the subtraction X-M without storing the result).
void CPU::CPX()
{
	u8 M = curr_instr.read_addr;
	flags.C = X >= M;
	flags.Z = X == M;
	u8 result = X - M;
	flags.N = result & 0x80;
}


// Compare the contents of the Y register with the contents of memory location (essentially performing the subtraction Y-M without storing the result)
void CPU::CPY()
{
	u8 M = curr_instr.read_addr;
	flags.C = Y >= M;
	flags.Z = Y == M;
	u8 result = Y - M;
	flags.N = result & 0x80;
}


// Subtract one from the value held at a specified memory location.
void CPU::DEC()
{
	u8 M = curr_instr.read_addr;
	M--;
	curr_instr.new_target = M;
	flags.Z = M == 0;
	flags.N = M & 0x80;
}


// Subtract one from the X register.
void CPU::DEX()
{
	X--;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Subtract one from the Y register.
void CPU::DEY()
{
	Y--;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


// Bitwise XOR between the accumulator and the contents of a memory location.
void CPU::EOR()
{
	u8 op = curr_instr.read_addr;
	A ^= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Add one to the value held at a specified memory location.
void CPU::INC()
{
	u8 M = curr_instr.read_addr;
	M++;
	curr_instr.new_target = M;
	flags.Z = M == 0;
	flags.N = M & 0x80;
}


// Add one to the X register.
void CPU::INX()
{
	X++;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Add one to the Y register.
void CPU::INY()
{
	Y++;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


// Set the program counter to the address specified by the operand.
void CPU::JMP()
{
	PC = curr_instr.addr;
}


// Push the program counter (minus one) on to stack the and set the program counter to the target memory address.
void CPU::JSR()
{
	PushWordToStack(PC - 1);
	PC = curr_instr.addr;
	// This instr takes 6 cycles, but due to the way that the StepAbsolute() function is built for instructions of type "implicit", all such instructions take 3 cycles.
	// So we need to wait an additional 3 cycles
	bus->WaitCycle(3);
}


// Load a byte of memory into the accumulator.
void CPU::LDA()
{
	u8 M = curr_instr.read_addr;
	A = M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Load a byte of memory into the X register.
void CPU::LDX()
{
	u8 M = curr_instr.read_addr;
	X = M;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Load a byte of memory into the Y register.
void CPU::LDY()
{
	u8 M = curr_instr.read_addr;
	Y = M;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


// Perform a logical shift one place to the right of the accumulator or the contents of a memory location. The bit that was in bit 0 is shifted into the carry flag. Bit 7 is set to zero.
void CPU::LSR()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target >> 1;
	curr_instr.new_target = new_target;
	flags.C = target & 1;
	flags.Z = new_target == 0;
	flags.N = new_target & 0x80;
}


// No operation
void CPU::NOP()
{

}


// Bitwise OR between the accumulator and the contents of a memory location.
void CPU::ORA()
{
	u8 M = curr_instr.read_addr;
	A |= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Push a copy of the accumulator on to the stack.
void CPU::PHA()
{
	PushByteToStack(A);
	// This instr takes 3 cycles, but due to the way that the StepImplied() function is built,
    //    all instr with implied addressing take 2 cycles.
	bus->WaitCycle();
}


// Push a copy of the status register on to the stack (with bit 4 set).
void CPU::PHP()
{
	PushByteToStack(GetStatusRegInstr(&CPU::PHP));
	bus->WaitCycle();
}


// Pull an 8-bit value from the stack and into the accumulator.
void CPU::PLA()
{
	A = PullByteFromStack();
	flags.Z = A == 0;
	flags.N = A & 0x80;
	// This instr takes 4 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	bus->WaitCycle(2);
}


// Pull an 8-bit value from the stack and into the staus register.
void CPU::PLP()
{
	// PLP changes the I flag after polling for interrupts (effectively when CPU::Update() is called next time)
	bool I_tmp = flags.I;
	SetStatusReg(PullByteFromStack());
	if (flags.I)
		set_I_on_next_update = true;
	else
		clear_I_on_next_update = true;
	flags.I = I_tmp;

	bus->WaitCycle(2);
}


// Move each of the bits in either the accumulator or the value held at a memory location one place to the left. Bit 0 is filled with the current value of the carry flag whilst the old bit 7 becomes the new carry flag value.
void CPU::ROL()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = (target << 1 | flags.C) & 0xFF;
	curr_instr.new_target = new_target;
	flags.C = target & 0x80;
	flags.Z = (new_target == 0 && curr_instr.addr_mode == AddrMode::Accumulator) ||
		(A == 0 && curr_instr.addr_mode != AddrMode::Accumulator);
	flags.N = new_target & 0x80;
}


// Move each of the bits in either the accumulator or the value held at a memory location one place to the right. Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
void CPU::ROR()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target >> 1 | flags.C << 7;
	curr_instr.new_target = new_target;
	flags.C = target & 1;
	flags.Z = (new_target == 0 && curr_instr.addr_mode == AddrMode::Accumulator) ||
		(A == 0 && curr_instr.addr_mode != AddrMode::Accumulator);
	flags.N = new_target & 0x80;
}


// Return from interrupt; pull the status register and program counter from the stack.
void CPU::RTI()
{
	SetStatusReg(PullByteFromStack());
	PC = PullWordFromStack();
	// This instr takes 6 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	bus->WaitCycle(4);
}


// Return from subrouting; pull the program counter (minus one) from the stack.
void CPU::RTS()
{
	PC = PullWordFromStack() + 1;
	// This instr takes 6 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	bus->WaitCycle(4);
}


// Subtract the contents of a memory location to the accumulator together with the NOT of the carry bit. If overflow occurs the carry bit is cleared.
void CPU::SBC()
{
	u8 op = curr_instr.read_addr + (1 - flags.C);
	flags.V = ((A & 0x7F) + ((u8)(0xFF - curr_instr.read_addr) & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + ((u8)(0xFF - curr_instr.read_addr)       ) + flags.C > 0xFF);
	flags.C = A > curr_instr.read_addr || A == curr_instr.read_addr && flags.C;
	A -= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Set the carry flag.
void CPU::SEC()
{
	flags.C = 1;
}


// Set the decimal mode flag.
void CPU::SED()
{
	flags.D = 1;
}


// Set the interrupt disable flag.
void CPU::SEI()
{
	// SEI sets the I flag after polling for interrupts (effectively when CPU::Update() is called next time)
	set_I_on_next_update = true;
}


// Store the contents of the accumulator into memory.
void CPU::STA()
{
	bus->Write(curr_instr.addr, A);
}


// Store the contents of the X register into memory.
void CPU::STX()
{
	bus->Write(curr_instr.addr, X);
}


// Store the contents of the Y register into memory.
void CPU::STY()
{
	bus->Write(curr_instr.addr, Y);
}


// Copy the current contents of the accumulator into the X register.
void CPU::TAX()
{
	X = A;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Copy the current contents of the accumulator into the Y register.
void CPU::TAY()
{
	Y = A;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


// Copy the current contents of the stack register into the X register.
void CPU::TSX()
{
	X = S;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Copy the current contents of the X register into the accumulator.
void CPU::TXA()
{
	A = X;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Copy the current contents of the X register into the stack register (flags are not affected).
void CPU::TXS()
{
	S = X;
}


// Copy the current contents of the Y register into the accumulator.
void CPU::TYA()
{
	A = Y;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; store A AND X AND (the high byte of addr + 1) at addr
void CPU::AHX()
{
	u8 op = (curr_instr.addr >> 8) + 1;
	bus->Write(curr_instr.addr, A & X & op);
}


// Unofficial instruction; combined AND and LSR
void CPU::ALR()
{
	AND();
	LSR();
}


// Unofficial instruction; AND with immediate addressing, where the carry flag is set to bit 7 of the result (equal to the negative flag after the AND)
void CPU::ANC()
{
	AND();
	flags.C = flags.N;
}


// Unofficial instruction; combined AND and ROR, but where the overflow and carry flags are set in particular ways
void CPU::ARR()
{
	// TODO: set flags.V and flags.C as they should be
	AND();
	ROR();
}


// Unofficial instruction; store A AND X at addr
void CPU::AXS()
{
	bus->Write(curr_instr.addr, A & X);
}


// Unofficial instruction; combined DEC and CMP
void CPU::DCP()
{
	DEC();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	CMP();
}


// Unofficial instruction; combined INC and SBC
void CPU::ISC()
{
	INC();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	SBC();
}


// Unofficial instruction; fused LDA and TSX instruction, where M AND S are put into A, X, S
void CPU::LAS()
{
	// TODO: not sure why S would be written to as well?
	u8 AND = curr_instr.read_addr & S;
	A = X = S = AND;
	flags.Z = AND == 0;
	flags.N = AND & 0x80;
}


// Unofficial instruction; combined LDA and LDX
void CPU::LAX()
{
	LDA();
	LDX();
}


// Unofficial instruction; combined ROL and AND
void CPU::RLA()
{
	ROL();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	AND();
}


// Unofficial instruction; combined ROR and ADC
void CPU::RRA()
{
	ROR();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	ADC();
}


// Unofficial instruction; same thing as AXS but with different addressing modes allowed
void CPU::SAX()
{
	AXS();
}


// Unofficial instruction; store X AND (high byte of addr + 1) at addr
void CPU::SHX()
{
	u8 op = (curr_instr.addr >> 8) + 1;
	bus->Write(curr_instr.addr, X & op);
}


// Unofficial instruction; store Y AND (high byte of addr + 1) at addr
void CPU::SHY()
{
	u8 op = (curr_instr.addr >> 8) + 1;
	bus->Write(curr_instr.addr, Y & op);
}


// Unofficial instruction; combined ASL and ORA
void CPU::SLO()
{
	ASL();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	ORA();
}


// Unofficial instruction; combined LSR and EOR
void CPU::SRE()
{
	LSR();
	curr_instr.read_addr = curr_instr.new_target; // The 2nd instr uses the result of the 1st one, but the result has not been written to memory yet
	EOR();
}


// Unofficial instruction; STop the Processor. 
void CPU::STP()
{
	stopped = true;
}


// Unofficial instruction; store A AND X in S and A AND X AND (high byte of addr + 1) at addr
void CPU::TAS()
{
	S = A & X;
	u8 op = (curr_instr.addr >> 8) + 1;
	bus->Write(curr_instr.addr, A & X & op);
}


// Unofficial instruction; highly unstable instruction that stores (A OR CONST) AND X AND oper into A, where CONST depends on things such as the temperature!
void CPU::XAA()
{
	// Use CONST = 0
	A &= X & curr_instr.read_addr;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&curr_instr, sizeof(InstrDetails));
	functor.fun(&A, sizeof(u8));
	functor.fun(&X, sizeof(u8));
	functor.fun(&Y, sizeof(u8));
	functor.fun(&S, sizeof(u8));
	functor.fun(&PC, sizeof(u16));
	functor.fun(&flags, sizeof(Flags));
}