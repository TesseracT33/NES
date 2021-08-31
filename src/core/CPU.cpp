#include "CPU.h"


CPU::CPU()
{
	BuildInstrTypeTable();
}


void CPU::BuildInstrTypeTable()
{
	auto GetInstrType = [&](instr_t instr)
	{
		// switch stmnt not possible, as 'instr' is not an integral or enum type
		// this only gets executed at program startup anyways, so the poor performance does not matter
		if (instr == &CPU::ADC || instr == &CPU::AND || instr == &CPU::BIT || instr == &CPU::CMP ||
			instr == &CPU::CPX || instr == &CPU::CPY || instr == &CPU::EOR || instr == &CPU::JMP ||
			instr == &CPU::JSR || instr == &CPU::LDA || instr == &CPU::LDX || instr == &CPU::LDY ||
			instr == &CPU::ORA || instr == &CPU::SBC)
			return InstrType::Read;

		if (instr == &CPU::STA || instr == &CPU::STX || instr == &CPU::STY)
			return InstrType::Write;

		if (instr == &CPU::ASL || instr == &CPU::DEC || instr == &CPU::INC || instr == &CPU::LSR ||
			instr == &CPU::ROL || instr == &CPU::ROR)
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

	IRQ = 1;
}


void CPU::Reset()
{
	cpu_clocks_since_reset = 0;
	all_ppu_regs_writable = false;

	PC = bus->Read(0xFFFC) | bus->Read(0xFFFD) << 8;
}


void CPU::Update()
{
	#ifdef DEBUG
		cpu_cycle_counter++;
	#endif

	if (!all_ppu_regs_writable && ++cpu_clocks_since_reset == cpu_clocks_until_all_ppu_regs_writable)
		all_ppu_regs_writable = true;

	if (oam_dma_transfer_active)
	{
		if (--oam_dma_cycles_until_finished == 0)
			oam_dma_transfer_active = false;
		return;
	}

	if (interrupt_is_being_serviced)
	{
		if (--cycles_until_interrupt_service_stops == 0)
		{
			interrupt_is_being_serviced = false;
			if (handled_interrupt_type == InterruptType::NMI)
				NMI_signal_raised = false;
			// todo: IRQ signal disable?
		}
		return;
	}

	// If an instruction is currently being executed
	if (curr_instr.instr_executing)
	{
		// Continue the execution of the instruction
		std::invoke(curr_instr.addr_mode_fun, this);
	}
	else
	{
		// Check for pending interrupts (NMI and IRQ); NMI has higher priority than IRQ
		// Interrupts are only polled after executing an instruction; multiple interrupts cannot be serviced in a row
		if (!interrupt_serviced_on_last_update)
		{
			if (NMI_signal_raised)
				ServiceInterrupt(InterruptType::NMI);
			else if (IRQ_signal_active && !flags.I)
				ServiceInterrupt(InterruptType::IRQ);
			return;
		}

		BeginInstruction();
		interrupt_serviced_on_last_update = false;
	}
}


void CPU::Set_OAM_DMA_Active()
{
	oam_dma_transfer_active = true;
	oam_dma_cycles_until_finished = odd_cpu_cycle ? 514 : 513;
}


void CPU::BeginInstruction()
{
	curr_instr.opcode = bus->Read(PC++);
	curr_instr.addr_mode = GetAddressingModeFromOpcode(curr_instr.opcode);
	curr_instr.addr_mode_fun = addr_mode_fun_table[static_cast<int>(curr_instr.addr_mode)];
	curr_instr.instr = instr_table[curr_instr.opcode];
	curr_instr.instr_executing = true;
	curr_instr.cycle = 1;
	curr_instr.additional_cycles = 0;

	#ifdef DEBUG
		char buf[100]{};
		sprintf(buf, "#instr %i \t #cycle %i \t PC: $%04X \t OP: $%02X \t S: $%02X \t A: $%02X \t X: $%02X \t Y: $%02X",
			instruction_counter++, cpu_cycle_counter, (int)(PC - 1), (int)curr_instr.opcode, (int)S, (int)A, (int)X, (int)Y);
		ofs << buf << std::endl;
	#endif
}


void CPU::StepImplicit()
{
	// Some instructions using implicit addressing take longer than two cycles (remaining amount is stored in curr_instr.additional_cycles)
	//    so we need to wait until these "finish".
	static bool instr_invoked = false;

	if (!instr_invoked)
	{
		std::invoke(curr_instr.instr, this);
		if (curr_instr.additional_cycles == 0)
			curr_instr.instr_executing = false;
		else
			instr_invoked = true;
	}
	else if (--curr_instr.additional_cycles == 0)
		curr_instr.instr_executing = instr_invoked = false;
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
		return;

	case 3:
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo;
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
		else if (curr_instr.instr_type == InstrType::Read)
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
	// Branch instructions (only these use relative addressing) take 2 + 1 or 2 + 2 cycles, depending on if the branch succeeds
	// However, all branching logic is inside of the Branch() function (including changing PC, which probably happens on cycle 3 on real HW?)
	// Not that it matters; neither the PC or the offset can be changed from the outside, if PC were changed in cycle 3 instead of 2.
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		std::invoke(curr_instr.instr, this);
		if (curr_instr.additional_cycles == 0)
			curr_instr.instr_executing = false;
		return;

	case 2:
		if (--curr_instr.additional_cycles == 0)
			curr_instr.instr_executing = false;
		return;

	case 3:
		curr_instr.instr_executing = false;
	}
}


void CPU::StepIndirect()
{
	// https://skilldrick.github.io/easy6502/#addressing
	switch (curr_instr.cycle++)
	{
	case 1:
		curr_instr.addr_lo = bus->Read(PC++);
		return;

	case 2:
		curr_instr.addr_hi = bus->Read(PC++);
		return;

	case 3:
	{
		curr_instr.addr = curr_instr.addr_hi << 8 | curr_instr.addr_lo;
		u8 addr_lo = bus->Read(curr_instr.addr);
		u8 addr_hi = addr_lo + 1;
		curr_instr.addr = addr_hi << 8 | addr_lo;
		return;
	}

	case 4:
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
	case 0x0A: return AddrMode::Accumulator;
	case 0x0B: return AddrMode::Immediate;
	case 0x0C: return (opcode == 0x6) ? AddrMode::Indirect : AddrMode::Absolute;
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
	case 0x18: return (opcode == 0x98) ? AddrMode::Accumulator : AddrMode::Implicit;
	case 0x19: return AddrMode::Absolute_Y;
	case 0x1A: return AddrMode::Implicit;
	case 0x1B: return AddrMode::Absolute_Y;
	case 0x1C:
	case 0x1D: return AddrMode::Absolute_X;
	case 0x1E:
	case 0x1F: return (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Absolute_Y : AddrMode::Absolute_X;
	}
}


void CPU::RequestNMIInterrupt()
{
	NMI_signal_raised = true;
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
	*/
	
	// Note: what happens in cycle 1 and 2 does not need to be emulated (beyond the timing)
	PushWordToStack(PC);

	// Interrupt hijacking: If both an NMI and an IRQ are pending, the NMI will be handled and the pending status of the IRQ forgotten
	// The same applies to IRQ and BRK; an IRQ can hijack a BRK
	if (asserted_interrupt_type == InterruptType::NMI || NMI_signal_raised)
	{
		PC = bus->Read(Bus::Addr::NMI_VEC) | bus->Read(Bus::Addr::NMI_VEC + 1) << 8;
		handled_interrupt_type = InterruptType::NMI;
	}
	else
	{
		PC = bus->Read(Bus::Addr::IRQ_BRK_VEC) | bus->Read(Bus::Addr::IRQ_BRK_VEC + 1) << 8;
		if (asserted_interrupt_type == InterruptType::IRQ || IRQ_signal_active && !flags.I)
			handled_interrupt_type = InterruptType::IRQ;
		else
			handled_interrupt_type = InterruptType::BRK;
	}

	PushByteToStack(GetStatusRegInterrupt());
	flags.I = 1;

	interrupt_is_being_serviced = interrupt_serviced_on_last_update = true;
	cycles_until_interrupt_service_stops = 6; // Not 7; the current cycle will already have passed

	if (handled_interrupt_type == InterruptType::BRK)
	{
		// If the interrupt servicing is forced from the BRK instruction, the B flag is also set, and the interrupt servicing takes one less cycle to perform than otherwise
		flags.B = 1;
		cycles_until_interrupt_service_stops--;
	}
}


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


void CPU::AND()
{
	u8 op = curr_instr.read_addr;
	A &= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::ASL()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target << 1 & 0xFF;
	curr_instr.new_target = new_target;
	flags.C = target & 0x80;
	flags.Z = (new_target == 0 && curr_instr.addr_mode == AddrMode::Accumulator) ||
		(A == 0 && curr_instr.addr_mode != AddrMode::Accumulator);
	flags.N = new_target & 0x80;
}


void CPU::BCC()
{
	Branch(!flags.C);
}


void CPU::BCS()
{
	Branch(flags.C);
}


void CPU::BEQ()
{
	Branch(flags.Z);
}


void CPU::BIT()
{
	u8 op = curr_instr.read_addr;
	flags.Z = A & op;
	flags.V = op & 0x40;
	flags.N = op & 0x80;
}


void CPU::BMI()
{
	Branch(flags.N);
}


void CPU::BNE()
{
	Branch(!flags.Z);
}


void CPU::BPL()
{
	Branch(!flags.N);
}


void CPU::BRK()
{
	ServiceInterrupt(InterruptType::BRK);
}


void CPU::BVC()
{
	Branch(!flags.V);
}


void CPU::BVS()
{
	Branch(flags.V);
}


void CPU::CLC()
{
	flags.C = 0;
}


void CPU::CLD()
{
	flags.D = 0;
}


void CPU::CLI()
{
	flags.I = 0;
}


void CPU::CLV()
{
	flags.V = 0;
}


void CPU::CMP()
{
	u8 M = curr_instr.read_addr;
	flags.C = A >= M;
	flags.Z = A == M;
	u8 result = A - M;
	flags.N = result & 0x80;
}


void CPU::CPX()
{
	u8 M = curr_instr.read_addr;
	flags.C = X >= M;
	flags.Z = X == M;
	u8 result = X - M;
	flags.N = result & 0x80;
}


void CPU::CPY()
{
	u8 M = curr_instr.read_addr;
	flags.C = Y >= M;
	flags.Z = Y == M;
	u8 result = Y - M;
	flags.N = result & 0x80;
}


void CPU::DEC()
{
	u8 M = curr_instr.read_addr;
	M--;
	curr_instr.new_target = M;
	flags.Z = M == 0;
	flags.N = M & 0x80;
}


void CPU::DEX()
{
	X--;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


void CPU::DEY()
{
	Y--;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


void CPU::EOR()
{
	u8 op = curr_instr.read_addr;
	A ^= op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::INC()
{
	u8 M = curr_instr.read_addr;
	M++;
	curr_instr.new_target = M;
	flags.Z = M == 0;
	flags.N = M & 0x80;
}


void CPU::INX()
{
	X++;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


void CPU::INY()
{
	Y++;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


void CPU::JMP()
{
	PC = curr_instr.addr;
	// todo NB
}


void CPU::JSR()
{
	PushWordToStack(PC - 1);
	PC = curr_instr.addr;
}


void CPU::LDA()
{
	u8 M = curr_instr.read_addr;
	A = M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::LDX()
{
	u8 M = curr_instr.read_addr;
	X = M;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


void CPU::LDY()
{
	u8 M = curr_instr.read_addr;
	Y = M;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


void CPU::LSR()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target >> 1;
	curr_instr.new_target = new_target;
	flags.C = target & 0x80;
	flags.Z = new_target == 0;
	flags.N = new_target & 0x80;
}


void CPU::NOP()
{

}


void CPU::ORA()
{
	u8 M = curr_instr.read_addr;
	A |= M;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::PHA()
{
	PushByteToStack(A);
	// This instr takes 3 cycles, but due to the way that the StepImplied() function is built,
    //    all instr with implied addressing take 2 cycles.
	curr_instr.additional_cycles = 1;
}


void CPU::PHP()
{
	PushByteToStack(GetStatusRegInstr(&CPU::PHP));
	curr_instr.additional_cycles = 1;
}


void CPU::PLA()
{
	A = PullByteFromStack();
	flags.Z = A == 0;
	flags.N = A & 0x80;
	// This instr takes 4 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	curr_instr.additional_cycles = 2;
}


void CPU::PLP()
{
	SetStatusReg(PullByteFromStack());
	curr_instr.additional_cycles = 2;
}


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


void CPU::ROR()
{
	u8 target = curr_instr.read_addr;
	u8 new_target = target >> 1 | flags.C << 7;
	curr_instr.new_target = new_target;
	flags.C = target & 0x80;
	flags.Z = (new_target == 0 && curr_instr.addr_mode == AddrMode::Accumulator) ||
		(A == 0 && curr_instr.addr_mode != AddrMode::Accumulator);
	flags.N = new_target & 0x80;
}


void CPU::RTI()
{
	SetStatusReg(PullByteFromStack());
	PC = PullWordFromStack();
	// This instr takes 6 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	curr_instr.additional_cycles = 4;
}


void CPU::RTS()
{
	PC = PullWordFromStack() + 1;
	// This instr takes 6 cycles, but due to the way that the StepImplied() function is built,
	//    all instr with implied addressing take 2 cycles.
	curr_instr.additional_cycles = 4;
}


void CPU::SBC()
{
	u8 op = (0xFF - curr_instr.read_addr) + flags.C;
	flags.V = ((A & 0x7F) + ((u8)(0xFF - curr_instr.read_addr) & 0x7F) + flags.C > 0x7F)
	        ^ ((A       ) + ((u8)(0xFF - curr_instr.read_addr)       ) + flags.C > 0xFF);
	flags.C = !(A + op > 0xFF);
	A += op;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::SEC()
{
	flags.C = 1;
}


void CPU::SED()
{
	flags.D = 1;
}


void CPU::SEI()
{
	flags.I = 1;
}


void CPU::STA()
{
	bus->Write(curr_instr.addr, A);
}


void CPU::STX()
{
	bus->Write(curr_instr.addr, X);
}


void CPU::STY()
{
	bus->Write(curr_instr.addr, Y);
}


void CPU::TAX()
{
	X = A;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


void CPU::TAY()
{
	Y = A;
	flags.Z = Y == 0;
	flags.N = Y & 0x80;
}


void CPU::TSX()
{
	X = S;
	flags.Z = X == 0;
	flags.N = X & 0x80;
}


void CPU::TXA()
{
	A = X;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::TXS()
{
	S = X;
	flags.Z = S == 0;
	flags.N = S & 0x80;
}


void CPU::TYA()
{
	A = Y;
	flags.Z = A == 0;
	flags.N = A & 0x80;
}


void CPU::AHX()
{

}


void CPU::ALR()
{
	AND();
	LSR();
}


void CPU::ANC()
{
	AND();
	flags.C = flags.N;
}


void CPU::ARR()
{

}


void CPU::AXS()
{

}


void CPU::DCP()
{
	DEC();
	CMP();
}


void CPU::DLC()
{

}


void CPU::ISC()
{
	INC();
	SBC();
}


void CPU::LAS()
{

}


void CPU::LAX()
{

}


void CPU::RLA()
{
	ROL();
	AND();
}


void CPU::RRA()
{
	ROR();
	ADC();
}


void CPU::SAX()
{
	u8 new_target = A & X;
	curr_instr.new_target = new_target;
}


void CPU::SHX()
{

}


void CPU::SHY()
{

}


void CPU::SLO()
{
	ASL();
	ORA(); // order of setting A not correct?
}


void CPU::SRE()
{
	LSR();
	EOR(); // order of setting A not correct?
}


void CPU::STP()
{

}


void CPU::TAS()
{
	S = A;
	flags.Z = S == 0;
	flags.N = S & 0x80;
}


void CPU::XAA()
{

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