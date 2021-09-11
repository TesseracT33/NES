#include "CPU.h"



void CPU::Power()
{
	// https://wiki.nesdev.com/w/index.php?title=CPU_power_up_state

	Reset();

	SetStatusReg(0x34);
	A = X = Y = 0;
	SP = 0xFD;
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
	oam_dma_transfer_pending = false;
	odd_cpu_cycle = true; // Not defined as false, for everytime that ReadCycle/WriteCycle is called, odd_cpu_cycle is inverted before doing the read/write.

	PC = bus->Read(Bus::Addr::RESET_VEC) | bus->Read(Bus::Addr::RESET_VEC + 1) << 8;
}


void CPU::Run()
{
	bus->cpu_cycle_counter = 0;
	const unsigned cycle_run_len = 20000;

	// Run the CPU for roughly a whole frame (exact timing is not important; synchronization is done by the APU).
	while (bus->cpu_cycle_counter < cycle_run_len)
	{
		if (stopped)
			WaitCycle(cycle_run_len - bus->cpu_cycle_counter);

		//if (!all_ppu_regs_writable && ++cpu_clocks_since_reset == cpu_clocks_until_all_ppu_regs_writable)
		//	all_ppu_regs_writable = true;

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

		if (oam_dma_transfer_pending)
		{
			PerformOAMDMATransfer();
		}
		else
		{
			ExecuteInstruction();

			// Check for pending interrupts (NMI and IRQ); NMI has higher priority than IRQ
			// Interrupts are only polled after executing an instruction; multiple interrupts cannot be serviced in a row
			if (NMI_signal_active)
				ServiceInterrupt(InterruptType::NMI);
			else if (IRQ_num_inputs > 0 && !flags.I)
				ServiceInterrupt(InterruptType::IRQ);
		}
	}
}


void CPU::StartOAMDMATransfer(u8 page, u8* oam_start_ptr)
{
	oam_dma_transfer_pending = true;
	oam_dma_base_addr = page << 8;
	this->oam_start_ptr = oam_start_ptr;
}


void CPU::PerformOAMDMATransfer()
{
	if (odd_cpu_cycle)
		WaitCycle();

	// 512 cycles
	for (u16 i = 0; i < 0x100; i++)
	{
		*(oam_start_ptr + i) = ReadCycle(oam_dma_base_addr + i);
		WaitCycle();
	}

	oam_dma_transfer_pending = false;
}


void CPU::ExecuteInstruction()
{
	curr_instr.opcode = ReadCycle(PC++);
	curr_instr.addr_mode = GetAddressingModeFromOpcode(curr_instr.opcode);
	curr_instr.addr_mode_fun = addr_mode_fun_table[static_cast<int>(curr_instr.addr_mode)];
	curr_instr.instr = instr_table[curr_instr.opcode];
	curr_instr.page_crossing_possible = curr_instr.page_crossed = false;

#ifdef DEBUG
	LogState(Action::Instruction);
#endif

	std::invoke(curr_instr.addr_mode_fun, this);
}


void CPU::ExecuteImplied()
{
	ReadCycle(PC);
	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteAccumulator()
{
	ReadCycle(PC);
	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteImmediate()
{
	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteZeroPage()
{
	curr_instr.addr = ReadCycle(PC++);
	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteZeroPageIndexed(u8& index_reg)
{
	curr_instr.addr = ReadCycle(PC++);
	ReadCycle(curr_instr.addr);
	curr_instr.addr = (curr_instr.addr + index_reg) & 0xFF;

	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteAbsolute()
{
	u8 addr_lo = ReadCycle(PC++);
	u8 addr_hi = ReadCycle(PC++);
	curr_instr.addr = addr_hi << 8 | addr_lo;

	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteAbsoluteIndexed(u8& index_reg)
{
	curr_instr.page_crossing_possible = true;

	u8 addr_lo = ReadCycle(PC++);
	u8 addr_hi = ReadCycle(PC++);
	curr_instr.page_crossed = addr_lo + index_reg > 0xFF;
	addr_lo += index_reg;
	curr_instr.addr = addr_hi << 8 | addr_lo;
	curr_instr.read_addr = ReadCycle(curr_instr.addr);
	if (curr_instr.page_crossed)
		curr_instr.addr = ((addr_hi + 1) & 0xFF) << 8 | addr_lo;

	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteRelative()
{
	curr_instr.addr = ReadCycle(PC++);
	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteIndirect()
{
	u8 addr_lo = ReadCycle(PC++);
	u8 addr_hi = ReadCycle(PC++);
	curr_instr.addr = addr_hi << 8 | addr_lo;
	u16 addr_tmp = ReadCycle(curr_instr.addr);

	// HW bug: if 'curr_instr.addr' is xyFF, then the upper byte of 'addr_tmp' is fetched from xy00, not (xy+1)00
	if (addr_lo == 0xFF)
		addr_tmp |= ReadCycle(addr_hi << 8) << 8;
	else
		addr_tmp |= ReadCycle(curr_instr.addr + 1) << 8;
	curr_instr.addr = addr_tmp;

	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteIndexedIndirect()
{
	curr_instr.addr = ReadCycle(PC++);
	ReadCycle(curr_instr.addr);
	curr_instr.addr = (curr_instr.addr + X) & 0xFF;
	curr_instr.read_addr = ReadCycle(curr_instr.addr);
	curr_instr.addr = (curr_instr.addr + 1) & 0xFF;
	curr_instr.addr = ReadCycle(curr_instr.addr) << 8 | curr_instr.read_addr;

	std::invoke(curr_instr.instr, this);
}


void CPU::ExecuteIndirectIndexed()
{
	curr_instr.page_crossing_possible = true;

	u8 addr_lo = ReadCycle(PC++);
	curr_instr.read_addr = ReadCycle(addr_lo);
	addr_lo++;
	u8 addr_hi = ReadCycle(addr_lo);
	curr_instr.page_crossed = curr_instr.read_addr + Y > 0xFF;
	addr_lo = curr_instr.read_addr + Y;
	curr_instr.addr = addr_hi << 8 | addr_lo;
	curr_instr.read_addr = ReadCycle(curr_instr.addr);
	if (curr_instr.page_crossed)
		curr_instr.addr = ((addr_hi + 1) & 0xFF) << 8 | addr_lo;

	std::invoke(curr_instr.instr, this);
}


CPU::AddrMode CPU::GetAddressingModeFromOpcode(u8 opcode) const
{
	switch (opcode & 0x1F)
	{
	case 0x00:
		if (opcode == 0x20)           return AddrMode::Absolute;
		if ((opcode & ~0x1F) >= 0x80) return AddrMode::Immediate;
		return AddrMode::Implied;
	case 0x01: return AddrMode::Indexed_indirect;
	case 0x02: return (opcode & ~0x1F) >= 0x80 ? AddrMode::Immediate : AddrMode::Implied;
	case 0x03: return AddrMode::Indexed_indirect;
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07: return AddrMode::Zero_page;
	case 0x08: return AddrMode::Implied;
	case 0x09: return AddrMode::Immediate;
	case 0x0A: return (opcode & ~0x1F) >= 0x80 ? AddrMode::Implied : AddrMode::Accumulator;
	case 0x0B: return AddrMode::Immediate;
	case 0x0C: return (opcode == 0x6C) ? AddrMode::Indirect : AddrMode::Absolute;
	case 0x0D:
	case 0x0E:
	case 0x0F: return AddrMode::Absolute;
	case 0x10: return AddrMode::Relative;
	case 0x11: return AddrMode::Indirect_indexed;
	case 0x12: return AddrMode::Implied;
	case 0x13: return AddrMode::Indirect_indexed;
	case 0x14:
	case 0x15: return AddrMode::Zero_page_X;
	case 0x16:
	case 0x17: return (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Zero_page_Y : AddrMode::Zero_page_X;
	case 0x18: return AddrMode::Implied;
	case 0x19: return AddrMode::Absolute_Y;
	case 0x1A: return AddrMode::Implied;
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
#ifdef DEBUG
	if (asserted_interrupt_type == InterruptType::NMI)
		LogState(Action::NMI);
#endif

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
	// note: the first two reads have already been handled in the BRK() function
	if (asserted_interrupt_type != InterruptType::BRK)
	{
		ReadCycle(PC);
		ReadCycle(PC);
	}

	// Cycles 3-4
	PushWordToStack(PC);

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
	PushByteToStack(GetStatusRegInterrupt());

	// cycles 6-7
	if (handled_interrupt_type == InterruptType::NMI)
	{
		PC = PC & 0xFF00 | ReadCycle(Bus::Addr::NMI_VEC);
		PC = PC & 0x00FF | ReadCycle(Bus::Addr::NMI_VEC + 1) << 8;
		NMI_signal_active = false; // todo: it's not entirely clear if this is the right place to clear this
	}
	else
	{
		PC = PC & 0xFF00 | ReadCycle(Bus::Addr::IRQ_BRK_VEC);
		PC = PC & 0x00FF | ReadCycle(Bus::Addr::IRQ_BRK_VEC + 1) << 8;
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
	u8 M = GetReadInstrOperand();
	u8 op = M + flags.C;
	flags.V = ((A & 0x7F) + (M & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + (M       ) + flags.C > 0xFF);
	flags.C = A + op > 0xFF;
	A += op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Bitwise AND between the accumulator and the contents of a memory location.
void CPU::AND()
{
	u8 M = GetReadInstrOperand();
	A &= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Shift all bits of the accumulator or the contents of a memory location one bit left. Bit 0 is cleared and bit 7 is placed in the carry flag.
void CPU::ASL()
{
	if (curr_instr.addr_mode == AddrMode::Accumulator)
	{
		flags.C = A & 0x80;
		A = A << 1 & 0xFF;
		flags.Z = A == 0;
		flags.N = A & 0x80;
	}
	else
	{
		u8 M = ReadCycle(curr_instr.addr);
		WriteCycle(curr_instr.addr, M);
		u8 new_M = M << 1 & 0xFF;
		WriteCycle(curr_instr.addr, new_M);
		flags.C = M & 0x80;
		flags.Z = new_M == 0;
		flags.N = new_M & 0x80;
	}
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
	u8 M = ReadCycle(curr_instr.addr);
	flags.Z = (A & M) == 0;
	flags.V = M & 0x40;
	flags.N = M & 0x80;
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
	u8 M = GetReadInstrOperand();
	flags.C = A >= M;
	flags.Z = A == M;
	u8 result = A - M;
	flags.N = result & 0x80;
}


// Compare the contents of the X register with the contents of a memory location (essentially performing the subtraction X-M without storing the result).
void CPU::CPX()
{
	u8 M = curr_instr.addr_mode == AddrMode::Immediate ? ReadCycle(PC++) : ReadCycle(curr_instr.addr);
	flags.C = X >= M;
	flags.Z = X == M;
	u8 result = X - M;
	flags.N = result & 0x80;
}


// Compare the contents of the Y register with the contents of memory location (essentially performing the subtraction Y-M without storing the result)
void CPU::CPY()
{
	u8 M = curr_instr.addr_mode == AddrMode::Immediate ? ReadCycle(PC++) : ReadCycle(curr_instr.addr);
	flags.C = Y >= M;
	flags.Z = Y == M;
	u8 result = Y - M;
	flags.N = result & 0x80;
}


// Subtract one from the value held at a specified memory location.
void CPU::DEC()
{
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M - 1;
	WriteCycle(curr_instr.addr, new_M);
	flags.Z = new_M == 0;
	flags.N = new_M & 0x80;
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
	u8 M = GetReadInstrOperand();
	A ^= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Add one to the value held at a specified memory location.
void CPU::INC()
{
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M + 1;
	WriteCycle(curr_instr.addr, new_M);
	flags.Z = new_M == 0;
	flags.N = new_M & 0x80;
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


// Push the program counter on to stack the and set the program counter to the target memory address.
void CPU::JSR()
{
	PushWordToStack(PC);
	PC = curr_instr.addr;
	WaitCycle();
}


// Load a byte of memory into the accumulator.
void CPU::LDA()
{
	u8 M = GetReadInstrOperand();
	A = M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Load a byte of memory into the X register.
void CPU::LDX()
{
	u8 M = GetReadInstrOperand();
	X = M;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Load a byte of memory into the Y register.
void CPU::LDY()
{
	u8 M = GetReadInstrOperand();
	Y = M;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


// Perform a logical shift one place to the right of the accumulator or the contents of a memory location. The bit that was in bit 0 is shifted into the carry flag. Bit 7 is set to zero.
void CPU::LSR()
{
	if (curr_instr.addr_mode == AddrMode::Accumulator)
	{
		flags.C = A & 1;
		A >>= 1;
		flags.Z = A == 0;
		flags.N = A & 0x80;
	}
	else
	{
		u8 M = ReadCycle(curr_instr.addr);
		WriteCycle(curr_instr.addr, M);
		u8 new_M = M >> 1;
		WriteCycle(curr_instr.addr, new_M);
		flags.C = M & 1;
		flags.Z = new_M == 0;
		flags.N = new_M & 0x80;
	}
}


// No operation
void CPU::NOP()
{

}


// Bitwise OR between the accumulator and the contents of a memory location.
void CPU::ORA()
{
	u8 M = GetReadInstrOperand();
	A |= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Push a copy of the accumulator on to the stack.
void CPU::PHA()
{
	PushByteToStack(A);
}


// Push a copy of the status register on to the stack (with bit 4 set).
void CPU::PHP()
{
	PushByteToStack(GetStatusRegInstr(&CPU::PHP));
}


// Pull an 8-bit value from the stack and into the accumulator.
void CPU::PLA()
{
	A = PullByteFromStack();
	flags.Z = A == 0;
	flags.N = A & 0x80;
	WaitCycle();
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

	WaitCycle();
}


// Move each of the bits in either the accumulator or the value held at a memory location one place to the left. Bit 0 is filled with the current value of the carry flag whilst the old bit 7 becomes the new carry flag value.
void CPU::ROL()
{
	if (curr_instr.addr_mode == AddrMode::Accumulator)
	{
		flags.C = A & 0x80;
		A = A << 1 | flags.C;
		flags.Z = A == 0;
		flags.N = A & 0x80;
	}
	else
	{
		u8 M = ReadCycle(curr_instr.addr);
		WriteCycle(curr_instr.addr, M);
		u8 new_M = M << 1 | flags.C;
		WriteCycle(curr_instr.addr, new_M);
		flags.C = M & 0x80;
		flags.Z = A == 0; // TODO: new_M == 0 ?
		flags.N = new_M & 0x80;
	}
}


// Move each of the bits in either the accumulator or the value held at a memory location one place to the right. Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
void CPU::ROR()
{
	if (curr_instr.addr_mode == AddrMode::Accumulator)
	{
		flags.C = A & 1;
		A = A >> 1 | flags.C << 7;
		flags.Z = A == 0; // TODO: new_M == 0 ?
		flags.N = A & 0x80;
	}
	else
	{
		u8 M = ReadCycle(curr_instr.addr);
		WriteCycle(curr_instr.addr, M);
		u8 new_M = M >> 1 | flags.C << 7;
		WriteCycle(curr_instr.addr, new_M);
		flags.C = M & 1;
		flags.Z = A == 0; // TODO: new_M == 0 ?
		flags.N = new_M & 0x80;
	}
}


// Return from interrupt; pull the status register and program counter from the stack.
void CPU::RTI()
{
	SetStatusReg(PullByteFromStack());
	PC = PullWordFromStack();
	WaitCycle();
}


// Return from subroutine; pull the program counter from the stack.
void CPU::RTS()
{
	PC = PullWordFromStack();
	WaitCycle(2);
}


// Subtract the contents of a memory location to the accumulator together with the NOT of the carry bit. If overflow occurs the carry bit is cleared.
void CPU::SBC()
{
	u8 M = GetReadInstrOperand();
	u8 op = M + (1 - flags.C);
	flags.V = ((A & 0x7F) + ((u8)(0xFF - M) & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + ((u8)(0xFF - M)       ) + flags.C > 0xFF);
	flags.C = A > M || A == M && flags.C;
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
	WriteCycle(curr_instr.addr, A);
}


// Store the contents of the X register into memory.
void CPU::STX()
{
	WriteCycle(curr_instr.addr, X);
}


// Store the contents of the Y register into memory.
void CPU::STY()
{
	WriteCycle(curr_instr.addr, Y);
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
	X = SP;
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
	SP = X;
}


// Copy the current contents of the Y register into the accumulator.
void CPU::TYA()
{
	A = Y;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; store A AND X AND (the high byte of addr + 1) at addr.
void CPU::AHX() // SHA / AXA
{
	u8 op = (curr_instr.addr >> 8) + 1;
	WriteCycle(curr_instr.addr, A & X & op);
}


// Unofficial instruction; combined AND and LSR with immediate addressing.
void CPU::ALR() // ASR
{
	// AND
	u8 M = ReadCycle(PC++);
	A &= M;
	// LSR
	flags.C = A & 1;
	A >>= 1;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; AND with immediate addressing, where the carry flag is set to bit 7 of the result (equal to the negative flag after the AND).
void CPU::ANC()
{
	u8 M = ReadCycle(PC++);
	A &= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
	flags.C = flags.N;
}


// Unofficial instruction; combined AND and ROR with immediate addressing, but where the overflow and carry flags are set in particular ways.
void CPU::ARR()
{
	// AND
	u8 M = ReadCycle(PC++);
	A &= M;
	// ROR
	A = A >> 1 | flags.C << 7;
	flags.C = A & 0x40;
	flags.V = flags.C ^ (A >> 5 & 1); 
}


// Unofficial instruction; store A AND X at addr.
void CPU::AXS() // SAX, AAX
{
	WriteCycle(curr_instr.addr, A & X);
}


// Unofficial instruction; combined DEC and CMP.
void CPU::DCP() // DCM
{
	// DEC
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M - 1;
	WriteCycle(curr_instr.addr, new_M);
	// CMP
	flags.C = A >= new_M;
	flags.Z = A == new_M;
	u8 result = A - new_M;
	flags.N = result & 0x80;
}


// Unofficial instruction; combined INC and SBC.
void CPU::ISC() // ISB, INS
{
	// TODO: https://www.masswerk.at/6502/6502_instruction_set.html#ANC gives four cycles for (indirect),Y
	// INC
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M + 1;
	WriteCycle(curr_instr.addr, new_M);
	// SBC
	u8 op = new_M + (1 - flags.C);
	flags.V = ((A & 0x7F) + ((u8)(0xFF - new_M) & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + ((u8)(0xFF - new_M)       ) + flags.C > 0xFF);
	flags.C = A > new_M || A == new_M && flags.C;
	A -= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; fused LDA and TSX instruction, where M AND S are put into A, X, S.
void CPU::LAS() // LAR
{
	u8 M = curr_instr.page_crossed ? ReadCycle(curr_instr.addr) : curr_instr.read_addr;
	A = X = SP = M & SP;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; combined LDA and LDX.
void CPU::LAX()
{
	// LDA
	u8 M = curr_instr.page_crossed ? ReadCycle(curr_instr.addr) : curr_instr.read_addr;
	A = M;
	// LDX
	X = M;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


// Unofficial instruction; combined ROL and AND.
void CPU::RLA()
{
	// TODO: Mesen states that this is a combined LSR and EOR
	// ROL
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M << 1 | flags.C;
	WriteCycle(curr_instr.addr, new_M);
	flags.C = M & 0x80;
	// AND
	A &= new_M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; combined ROR and ADC.
void CPU::RRA()
{
	// ROR
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M >> 1 | flags.C << 7;
	WriteCycle(curr_instr.addr, new_M);
	// ADC
	u8 op = new_M + flags.C;
	flags.V = ((A & 0x7F) + (new_M & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + (new_M       ) + flags.C > 0xFF);
	flags.C = A + op > 0xFF;
	A += op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; same thing as AXS but with different addressing modes allowed.
void CPU::SAX()
{
	// TODO remove this
	AXS();
}


// Unofficial instruction; store X AND (high byte of addr + 1) at addr.
void CPU::SHX()
{
	u8 op = (curr_instr.addr >> 8) + 1;
	WriteCycle(curr_instr.addr, X & op);
}


// Unofficial instruction; store Y AND (high byte of addr + 1) at addr.
void CPU::SHY()
{
	u8 op = (curr_instr.addr >> 8) + 1;
	WriteCycle(curr_instr.addr, Y & op);
}


// Unofficial instruction; combined ASL and ORA.
void CPU::SLO()
{
	// ASL
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M << 1 & 0xFF;
	WriteCycle(curr_instr.addr, new_M);
	flags.C = M & 0x80;
	// ORA
	A |= new_M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; combined LSR and EOR.
void CPU::SRE() // LSE
{
	// LSR
	u8 M = ReadCycle(curr_instr.addr);
	WriteCycle(curr_instr.addr, M);
	u8 new_M = M >> 1;
	WriteCycle(curr_instr.addr, new_M);
	flags.C = M & 1;
	// EOR
	A ^= new_M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


// Unofficial instruction; STop the Processor. 
void CPU::STP()
{
	stopped = true;
}


// Unofficial instruction; store A AND X in SP and A AND X AND (high byte of addr + 1) at addr.
void CPU::TAS() // XAS, SHS
{
	SP = A & X;
	u8 op = (curr_instr.addr >> 8) + 1;
	WriteCycle(curr_instr.addr, A & X & op);
}


// Unofficial instruction; highly unstable instruction that stores (A OR CONST) AND X AND M into A, where CONST depends on things such as the temperature!
void CPU::XAA()
{
	// Use CONST = 0
	u8 M = ReadCycle(PC++);
	A &= X & M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&curr_instr, sizeof(InstrDetails));
	functor.fun(&A, sizeof(u8));
	functor.fun(&X, sizeof(u8));
	functor.fun(&Y, sizeof(u8));
	functor.fun(&SP, sizeof(u8));
	functor.fun(&PC, sizeof(u16));
	functor.fun(&flags, sizeof(Flags));
}


void CPU::LogState(Action action)
{
	bool nmi = action == Action::NMI;
	Logging::ReportCpuState(A, X, Y, GetStatusRegInterrupt(), curr_instr.opcode, SP, PC - 1, bus->total_cpu_cycle_counter - 1, nmi);
	bus->update_logging_on_next_cycle = true;
}