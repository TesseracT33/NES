module CPU;

import Bus;
import Debug;
import System;
import PPU;

namespace CPU
{
	void PowerOn()
	{
		Reset(false /* do not jump to reset vector */);
		SetStatusReg(0x34);
		A = X = Y = 0;
		sp = 0xFD;
	}


	void Reset(const bool jump_to_reset_vector)
	{
		odd_cpu_cycle = false;
		stopped = false;
		nmi_line = polled_nmi_line = prev_polled_nmi_line = 1;
		need_nmi = polled_need_nmi = false;
		irq_line = 0xFF;
		need_irq = polled_need_irq = false;
		write_to_irq_disable_flag_before_next_instr = false;
		if (jump_to_reset_vector) {
			pc = ReadWord(Bus::Addr::RESET_VEC);
		}
	}


	void RunStartUpCycles()
	{
		/* Call this function after reset/initialization, where eight cycles pass before the CPU starts executing instructions.
		   Two of them pass above from when the initial program counter is fetched.
		   During the other ones, the stack pointer is decremented three times to $FD, dummy reads are made, etc.
		   These things have already been taken care of in the PowerOn function, or don't need to be emulated. */
		for (int i = 0; i < 6; ++i) {
			WaitCycle();
		}
		pc = ReadWord(Bus::Addr::RESET_VEC);
	}


	void Run()
	{
		/* Run the CPU for 10,000 cycles. A frame is roughly 30,000 cpu cycles. 
		Exact timing is not important; audio/video synchronization is done by the APU. */
		cpu_cycle_counter = 0; /* The ReadCycle/WriteCycle/WaitCycle functions increment this variable. */
		if (stopped) {
			while (cpu_cycle_counter < cycle_run_len) {
				WaitCycle();
			}
		}
		else {
			while (cpu_cycle_counter < cycle_run_len) {
				if (write_to_irq_disable_flag_before_next_instr) {
					status.irq_disable = bit_to_write_to_irq_disable_flag;
					write_to_irq_disable_flag_before_next_instr = false;
				}
				FetchDecodeExecuteInstruction();
				// Check for pending interrupts (NMI and IRQ); NMI has higher priority than IRQ
				// Interrupts are only polled after executing an instruction; multiple interrupts cannot be serviced in a row
				if (polled_need_nmi) {
					ServiceInterrupt<InterruptType::NMI>();
				}
				else if (polled_need_irq && !status.irq_disable) {
					ServiceInterrupt<InterruptType::IRQ>();
				}
			}
		}
	}


	void Stall()
	{
		/* Called from the APU class when the DMC memory reader fetches a new byte sample.
		* The CPU will then stop executing for a few cycles.
		* Note: Components are always stepped in the following order: CPU, APU, PPU.
		* This function was called from the APU. However, calling WaitCycle results in a stepping of 
		* the APU, and the PPU. Thus, we should step the PPU manually once, before calling WaitCycle. */
		PPU::Update();
		for (int i = 0; i < 4; ++i) {
			WaitCycle();
		}
	}


	void PerformOamDmaTransfer(u8 page, u8* oam_start_ptr, u8 offset)
	{
		u16 src_addr = page << 8;
		if constexpr (Debug::log_dma) {
			Debug::LogDma(src_addr);
		}
		WaitCycle();
		if (odd_cpu_cycle) {
			WaitCycle();
		}
		// 512 cycles in total.
		// If the write addr offset is > 0, then we will wrap around to the start of OAM again once we hit the end.
		for (u16 i = 0; i < 0x100 - offset; ++i) {
			*(oam_start_ptr + offset + i) = ReadCycle(src_addr + i);
			WaitCycle();
		}
		for (u16 i = 0; i < offset; ++i) {
			*(oam_start_ptr + i) = ReadCycle(src_addr + 0x100 - offset + i);
			WaitCycle();
		}
	}


	void PollInterruptInputs()
	{
		/* This function is called from within the PPU update function, 2/3 into each cpu cycle.
		   The NMI input is connected to an edge detector, which polls the status of the NMI line during the second half of each cpu cycle.
		   It raises an internal signal if the input goes from being high during one cycle to being low during the next.
		   The internal signal goes high during the first half of the cycle that follows the one where the edge is detected, and stays high until the NMI has been handled.
		   Note: 'need_NMI' is set to true as soon as the internal signal is raised, while 'polled_need_NMI' is updated to 'need_NMI' only one cycle after this. 'need_NMI_polled' is what determines whether to service an NMI after an instruction. */
		prev_polled_nmi_line = polled_nmi_line;
		polled_nmi_line = nmi_line;
		if (polled_nmi_line == 0 && prev_polled_nmi_line == 1) {
			need_nmi = true;
		}
		/* The IRQ input is connected to a level detector, which polls the status of the IRQ line during the second half of each cpu cycle.
		   If a low level is detected (at least one bit of IRQ_line is clear), it raises an internal signal during the first half of the next cycle, which remains high for that cycle only. */
		need_irq = irq_line != 0xFF;
	}


	void PollInterruptOutputs()
	{
		/* This function is called at the start of each CPU cycle.
		   need_NMI/need_IRQ signifies that we need to service an interrupt.
		   However, on real HW, these "signals" are only polled during the last cycle of an instruction. */
		polled_need_nmi = need_nmi;
		polled_need_irq = need_irq;
	}


	void SetIrqLow(IrqSource source_mask)
	{
		irq_line &= ~std::to_underlying(source_mask);
	}


	void SetIrqHigh(IrqSource source_mask)
	{
		irq_line |= std::to_underlying(source_mask);
	}


	void SetNmiLow()
	{
		nmi_line = 0;
	}


	void SetNmiHigh()
	{
		nmi_line = 1;
	}


	void StartCycle()
	{
		++cpu_cycle_counter;
		odd_cpu_cycle = !odd_cpu_cycle;
		PollInterruptOutputs();
	}


	u8 ReadCycle(u16 addr)
	{
		StartCycle();
		u8 value = Bus::Read(addr);
		System::StepAllComponentsButCpu();
		return value;
	}


	void WriteCycle(u16 addr, u8 data)
	{
		StartCycle();
		Bus::Write(addr, data);
		System::StepAllComponentsButCpu();
	}


	void WaitCycle()
	{
		StartCycle();
		System::StepAllComponentsButCpu();
	}


	void PushByteToStack(u8 byte)
	{
		WriteCycle(0x0100 | sp--, byte);
	}


	void PushWordToStack(u16 word)
	{
		WriteCycle(0x0100 | sp--, word >> 8);
		WriteCycle(0x0100 | sp--, word & 0xFF);
	}


	u8 PullByteFromStack()
	{
		return ReadCycle(0x0100 | ++sp);
	}


	u16 PullWordFromStack()
	{
		u8 lo = ReadCycle(0x0100 | ++sp);
		u8 hi = ReadCycle(0x0100 | ++sp);
		return hi << 8 | lo;
	}


	u16 ReadWord(u16 addr)
	{
		u8 lo = ReadCycle(addr);
		u8 hi = ReadCycle(addr + 1);
		return hi << 8 | lo;
	}


	void Branch(bool cond)
	{
		if (cond) {
			s8 offset = addr & 0xFF;
			// +1 cycle if branch succeeds, +2 if to a new page
			WaitCycle();
			if ((pc & 0xFF00) != ((pc + offset) & 0xFF00)) {
				WaitCycle();
			}
			pc += offset;
		}
	}


	// called when an instruction wants access to the status register
	template<Instruction instr>
	u8 GetStatusRegInstr()
	{
		static constexpr bool bit4 = instr == BRK || instr == PHP;
		return status.neg        << 7
			| status.overflow    << 6
			| 1                  << 5
			| bit4               << 4
			| status.decimal     << 3 
			| status.irq_disable << 2
			| status.zero        << 1
			| status.carry       << 0;
	}


	// called when an interrupt is being serviced and the status register is pushed to the stack
	u8 GetStatusRegInterrupt()
	{
		return status.neg        << 7
			| status.overflow    << 6
			| 1                  << 5 
			| status.decimal     << 3
			| status.irq_disable << 2
			| status.zero        << 1
			| status.carry       << 0;
	}


	void SetStatusReg(u8 value)
	{
		/* Keep the value of the upper bit of B (bit 5) */
		status.neg         = value & 0x80;
		status.overflow    = value & 0x40;
		status.b           = value & 0x10;
		status.decimal     = value & 0x08;
		status.irq_disable = value & 0x04;
		status.zero        = value & 0x02;
		status.carry       = value & 0x01;
	}


	void FetchDecodeExecuteInstruction()
	{
		opcode = ReadCycle(pc);
		if constexpr (Debug::log_instr) {
			Debug::LogInstr(
				opcode,
				A,
				X,
				Y,
				GetStatusRegInterrupt() & ~0x20,
				sp,
				pc
			);
		}
		pc++;
		instr_table[opcode]();
	}


	template<InterruptType asserted_interrupt_type>
	void ServiceInterrupt()
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
		  Moreover, on step 5, P is pushed with the B flag set.
		*/

		// Cycles 1-2. If the interrupt source is the BRK instruction, then the first two reads have already been handled (in the BRK() function).
		if constexpr (asserted_interrupt_type == InterruptType::BRK) { 
			/* commenting this out makes cpu_timing_test_6 fail. */
			//ReadCycle(PC++); /* Dummy read */
			//ReadCycle(PC++); /* Dummy read */
		}
		else {
			ReadCycle(pc); /* Dummy read */
			ReadCycle(pc); /* Dummy read */
		}

		// Cycles 3-4
		PushWordToStack(pc);

		InterruptType handled_interrupt_type;
		// Interrupt hijacking: If both an NMI and an IRQ are pending, the NMI will be handled and the pending status of the IRQ forgotten
		// The same applies to IRQ and BRK; an IRQ can hijack a BRK
		if (asserted_interrupt_type == InterruptType::NMI || polled_need_nmi) {
			handled_interrupt_type = InterruptType::NMI;
			need_irq = polled_need_irq = false; /* The possible pending status of the IRQ is forgotten */
		}
		else {
			if (asserted_interrupt_type == InterruptType::IRQ || polled_need_irq && !status.irq_disable) {
				handled_interrupt_type = InterruptType::IRQ;
			}
			else {
				handled_interrupt_type = InterruptType::BRK;
			}
		}

		// cycle 5
		PushByteToStack(GetStatusRegInterrupt());

		// cycles 6-7
		if (handled_interrupt_type == InterruptType::NMI) {
			pc = ReadWord(Bus::Addr::NMI_VEC);
			need_nmi = polled_need_nmi = false; // todo: it's not entirely clear if this is the right place to clear this.
			nmi_line = polled_nmi_line = prev_polled_nmi_line = 1; /* TODO should NMI line even be set, or is it the job of the interrupt handler? */
		}
		else {
			pc = ReadWord(Bus::Addr::IRQ_BRK_VEC);
			need_irq = polled_need_irq = false;
		}

		status.irq_disable = 1; // it's OK if this is set in cycle 7 instead of 6; the flag isn't used until after this function returns

		if (handled_interrupt_type == InterruptType::BRK) {
			// If the interrupt servicing is forced from the BRK instruction, the B flag is also set
			// According to http://obelisk.me.uk/6502/reference.html#BRK, the flag should be set after pushing the processor status on the stack, and not before
			status.b = 1;
		}
	}


	// Add the contents of a memory location to the accumulator together with the carry bit. If overflow occurs the carry bit is set.
	void ADC()
	{
		u16 sum = A + operand + status.carry;
		status.carry = sum > 0xFF;
		status.overflow = (A ^ sum) & (operand ^ sum) & 0x80;
		A = sum & 0xFF;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Bitwise AND between the accumulator and the contents of a memory location.
	void AND()
	{
		A &= operand;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Shift all bits of the accumulator one bit left. Bit 0 is cleared and bit 7 is placed in the carry flag.
	void ASL_A()
	{
		status.carry = A & 0x80;
		A <<= 1;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Shift all bits of the contents of a memory location one bit left. Bit 0 is cleared and bit 7 is placed in the carry flag.
	void ASL_M()
	{
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand << 1;
		WriteCycle(addr, result);
		status.carry = operand & 0x80;
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// If the carry flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
	void BCC()
	{
		Branch(!status.carry);
	}


	// If the carry flag is set then add the relative displacement to the program counter to cause a branch to a new location.
	void BCS()
	{
		Branch(status.carry);
	}


	// If the zero flag is set then add the relative displacement to the program counter to cause a branch to a new location.
	void BEQ()
	{
		Branch(status.zero);
	}


	// Check the bitwise AND between the accumulator and the contents of a memory location, and set the status flags accordingly.
	void BIT()
	{
		status.zero = (A & operand) == 0;
		status.overflow = operand & 0x40; /* Note: this does not depend on the AND */
		status.neg = operand & 0x80;
	}


	// If the negative flag is set then add the relative displacement to the program counter to cause a branch to a new location.
	void BMI()
	{
		Branch(status.neg);
	}


	// If the zero flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
	void BNE()
	{
		Branch(!status.zero);
	}


	// If the negative flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
	void BPL()
	{
		Branch(!status.neg);
	}


	// Force the generation of an interrupt request. Additionally, the break flag is set.
	void BRK()
	{
		ServiceInterrupt<InterruptType::BRK>();
	}


	// If the overflow flag is clear then add the relative displacement to the program counter to cause a branch to a new location.
	void BVC()
	{
		Branch(!status.overflow);
	}


	// If the overflow flag is set then add the relative displacement to the program counter to cause a branch to a new location.
	void BVS()
	{
		Branch(status.overflow);
	}


	// Clear the carry flag.
	void CLC()
	{
		status.carry = 0;
	}


	// Clear the decimal mode flag.
	void CLD()
	{
		status.decimal = 0;
	}


	// Clear the interrupt disable flag.
	void CLI()
	{
		// CLI clears the I flag after polling for interrupts (effectively when Update() is called next time)
		write_to_irq_disable_flag_before_next_instr = true;
		bit_to_write_to_irq_disable_flag = 0;
	}


	// Clear the overflow flag.
	void CLV()
	{
		status.overflow = 0;
	}


	// Compare the contents of the accumulator with the contents of a memory location (essentially performing the subtraction A-M without storing the result).
	void CMP()
	{
		status.carry = A >= operand;
		status.zero = A == operand;
		u8 result = A - operand;
		status.neg = result & 0x80;
	}


	// Compare the contents of the X register with the contents of a memory location (essentially performing the subtraction X-M without storing the result).
	void CPX()
	{
		status.carry = X >= operand;
		status.zero = X == operand;
		u8 result = X - operand;
		status.neg = result & 0x80;
	}


	// Compare the contents of the Y register with the contents of memory location (essentially performing the subtraction Y-M without storing the result)
	void CPY()
	{
		status.carry = Y >= operand;
		status.zero = Y == operand;
		u8 result = Y - operand;
		status.neg = result & 0x80;
	}


	// Subtract one from the value held at a specified memory location.
	void DEC()
	{
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand - 1;
		WriteCycle(addr, result);
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// Subtract one from the X register.
	void DEX()
	{
		--X;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Subtract one from the Y register.
	void DEY()
	{
		--Y;
		status.zero = Y == 0;
		status.neg = Y & 0x80;
	}


	// Bitwise XOR between the accumulator and the contents of a memory location.
	void EOR()
	{
		A ^= operand;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Add one to the value held at a specified memory location.
	void INC()
	{
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand + 1;
		WriteCycle(addr, result);
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// Add one to the X register.
	void INX()
	{
		++X;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Add one to the Y register.
	void INY()
	{
		++Y;
		status.zero = Y == 0;
		status.neg = Y & 0x80;
	}


	// Set the program counter to the address specified by the operand.
	void JMP()
	{
		pc = addr;
	}


	// Jump to subroutine; push the program counter (minus one) on to the stack and set the program counter to the target memory address.
	void JSR()
	{
		PushWordToStack(pc - 1);
		pc = addr;
		WaitCycle();
	}


	// Load a byte of memory into the accumulator.
	void LDA()
	{
		A = operand;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Load a byte of memory into the X register.
	void LDX()
	{
		X = operand;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Load a byte of memory into the Y register.
	void LDY()
	{
		Y = operand;
		status.zero = Y == 0;
		status.neg = Y & 0x80;
	}


	// Perform a logical shift one place to the right of the accumulator. The bit that was in bit 0 is shifted into the carry flag. Bit 7 is set to zero.
	void LSR_A()
	{
		status.carry = A & 1;
		A >>= 1;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Perform a logical shift one place to the right of the contents of a memory location. The bit that was in bit 0 is shifted into the carry flag. Bit 7 is set to zero.
	void LSR_M()
	{
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand >> 1;
		WriteCycle(addr, result);
		status.carry = operand & 1;
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// No operation
	void NOP()
	{

	}


	// Bitwise OR between the accumulator and the contents of a memory location.
	void ORA()
	{
		A |= operand;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Push a copy of the accumulator on to the stack.
	void PHA()
	{
		PushByteToStack(A);
	}


	// Push a copy of the status register on to the stack (with bit 4 set).
	void PHP()
	{
		PushByteToStack(GetStatusRegInstr<PHP>());
	}


	// Pull an 8-bit value from the stack and into the accumulator.
	void PLA()
	{
		A = PullByteFromStack();
		status.zero = A == 0;
		status.neg = A & 0x80;
		WaitCycle();
	}


	// Pull an 8-bit value from the stack and into the staus register.
	void PLP()
	{
		// PLP changes the I flag after polling for interrupts (effectively when Update() is called next time)
		bool I_tmp = status.irq_disable;
		SetStatusReg(PullByteFromStack());
		write_to_irq_disable_flag_before_next_instr = true;
		bit_to_write_to_irq_disable_flag = status.irq_disable;
		status.irq_disable = I_tmp;
		WaitCycle();
	}


	// Move each of the bits in the accumulator one place to the left. Bit 0 is filled with the current value of the carry flag whilst the old bit 7 becomes the new carry flag value.
	void ROL_A()
	{
		bool new_carry = A & 0x80;
		A = A << 1 | status.carry;
		status.carry = new_carry;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Move each of the bits in the value held at a memory location one place to the left. Bit 0 is filled with the current value of the carry flag whilst the old bit 7 becomes the new carry flag value.
	void ROL_M()
	{
		WriteCycle(addr, operand); /* Dummy write */
		bool new_carry = operand & 0x80;
		u8 result = operand << 1 | status.carry;
		WriteCycle(addr, result);
		status.carry = new_carry;
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// Move each of the bits in the accumulator one place to the right. Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
	void ROR_A()
	{
		bool new_carry = A & 1;
		A = A >> 1 | status.carry << 7;
		status.carry = new_carry;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Move each of the bits in the value held at a memory location one place to the right. Bit 7 is filled with the current value of the carry flag whilst the old bit 0 becomes the new carry flag value.
	void ROR_M()
	{
		WriteCycle(addr, operand); /* Dummy write */
		bool new_carry = operand & 1;
		u8 result = operand >> 1 | status.carry << 7;
		WriteCycle(addr, result);
		status.carry = new_carry;
		status.zero = result == 0;
		status.neg = result & 0x80;
	}


	// Return from interrupt; pull the status register and program counter from the stack.
	void RTI()
	{
		SetStatusReg(PullByteFromStack());
		pc = PullWordFromStack();
		WaitCycle();
	}


	// Return from subroutine; pull the program counter (minus one) from the stack.
	void RTS()
	{
		pc = PullWordFromStack() + 1;
		WaitCycle();
		WaitCycle();
	}


	// Subtract the contents of a memory location to the accumulator together with the NOT of the carry bit. If overflow occurs the carry bit is cleared.
	void SBC()
	{
		// SBC is equivalent to ADC, but where the operand has been XORed with $FF.
		operand ^= 0xFF;
		u16 sum = A + operand + status.carry;
		status.carry = sum > 0xFF;
		status.overflow = (A ^ sum) & (operand ^ sum) & 0x80;
		A = sum & 0xFF;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Set the carry flag.
	void SEC()
	{
		status.carry = 1;
	}


	// Set the decimal mode flag.
	void SED()
	{
		status.decimal = 1;
	}


	// Set the interrupt disable flag.
	void SEI()
	{
		// SEI sets the I flag after polling for interrupts (effectively when Update() is called next time)
		write_to_irq_disable_flag_before_next_instr = true;
		bit_to_write_to_irq_disable_flag = 1;
	}


	// Store the contents of the accumulator into memory.
	void STA()
	{
		WriteCycle(addr, A);
	}


	// Store the contents of the X register into memory.
	void STX()
	{
		WriteCycle(addr, X);
	}


	// Store the contents of the Y register into memory.
	void STY()
	{
		WriteCycle(addr, Y);
	}


	// Copy the current contents of the accumulator into the X register.
	void TAX()
	{
		X = A;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Copy the current contents of the accumulator into the Y register.
	void TAY()
	{
		Y = A;
		status.zero = Y == 0;
		status.neg = Y & 0x80;
	}


	// Copy the current contents of the stack register into the X register.
	void TSX()
	{
		X = sp;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Copy the current contents of the X register into the accumulator.
	void TXA()
	{
		A = X;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Copy the current contents of the X register into the stack register (flags are not affected).
	void TXS()
	{
		sp = X;
	}


	// Copy the current contents of the Y register into the accumulator.
	void TYA()
	{
		A = Y;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; store A AND X AND (the high byte of addr + 1) at addr.
	void AHX() // SHA / AXA
	{
		u8 op = (addr >> 8) + 1;
		WriteCycle(addr, A & X & op);
	}


	// Unofficial instruction; combined AND and LSR with immediate addressing.
	void ALR() // ASR
	{
		// AND
		operand = read_addr;
		A &= operand;
		// LSR
		status.carry = A & 1;
		A >>= 1;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; AND with immediate addressing, where the carry flag is set to bit 7 of the result (equal to the negative flag after the AND).
	void ANC()
	{
		u8 M = read_addr;
		A &= M;
		status.zero = A == 0;
		status.neg = A & 0x80;
		status.carry = status.neg;
	}


	// Unofficial instruction; combined AND with immediate addressing and ROR of the accumulator, but where the overflow and carry flags are set in particular ways.
	void ARR()
	{
		u8 M = read_addr;
		A = (A & M) >> 1 | status.carry << 7;
		status.zero = A == 0;
		status.neg = A & 0x80;
		status.carry = A & 0x40; /* Source: Mesen source code; CPU.h */
		status.overflow = status.carry ^ (A >> 5 & 0x01); /* Source: Mesen source code; CPU.h */
	}


	// Unofficial instruction; combined CMP and DEX with immediate addressing
	void AXS() // SAX, AAX
	{
		u8 M = read_addr;
		status.carry = (A & X) >= M;
		status.zero = (A & X) == M;
		u8 result = (A & X) - M;
		status.neg = result & 0x80;
		X = result; /* Source: Mesen source code; CPU.h */
	}


	// Unofficial instruction; combined DEC and CMP.
	void DCP() // DCM
	{
		// DEC
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand - 1;
		WriteCycle(addr, result);
		// CMP
		status.carry = A >= result;
		status.zero = A == result;
		result = A - result;
		status.neg = result & 0x80;
	}


	// Unofficial instruction; combined INC and SBC.
	void ISC() // ISB, INS
	{
		// TODO: https://www.masswerk.at/6502/6502_instruction_set.html#ISC gives four cycles for (indirect),Y
		// INC
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand + 1;
		WriteCycle(addr, result);
		// SBC. Equivalent to ADC, but where the operand has been XORed with $FF.
		result ^= 0xFF;
		u16 sum = A + result + status.carry;
		status.carry = sum > 0xFF;
		status.overflow = (A ^ sum) & (result ^ sum) & 0x80;
		A = sum & 0xFF;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; fused LDA and TSX instruction, where M AND S are put into A, X, S.
	void LAS() // LAR
	{
		A = X = sp = operand & sp;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; combined LDA and LDX.
	void LAX()
	{
		// LDA
		A = operand;
		// LDX
		X = operand;
		status.zero = X == 0;
		status.neg = X & 0x80;
	}


	// Unofficial instruction; combined ROL and AND.
	void RLA()
	{
		// TODO: Mesen states that this is a combined LSR and EOR
		// ROL
		WriteCycle(addr, operand); /* Dummy write */
		bool new_carry = operand & 0x80;
		u8 result = operand << 1 | status.carry;
		WriteCycle(addr, result);
		status.carry = new_carry;
		// AND
		A &= result;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; combined ROR and ADC.
	void RRA()
	{
		// ROR
		WriteCycle(addr, operand); /* Dummy write */
		bool new_carry = operand & 1;
		u8 result = operand >> 1 | status.carry << 7;
		WriteCycle(addr, result);
		status.carry = new_carry;
		// ADC
		u16 sum = A + result + status.carry;
		status.carry = sum > 0xFF;
		status.overflow = (A ^ sum) & (result ^ sum) & 0x80;
		A = sum & 0xFF;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; store A AND X at addr.
	void SAX()
	{
		WriteCycle(addr, A & X);
	}


	// Unofficial instruction; if H := [high byte of addr] + 1 and L := [low byte of addr], store X AND H at address [X AND H] << 8 | L.
	void SHX()
	{
		u8 AND = ((addr >> 8) + 1) & X;
		u8 addr_lo = addr & 0xFF;
		u8 addr_hi = AND;
		WriteCycle(addr_lo | addr_hi << 8, AND);
	}


	// Unofficial instruction; if H := [high byte of addr] + 1 and L := [low byte of addr], store Y AND H at address [Y AND H] << 8 | L.
	void SHY()
	{
		u8 AND = ((addr >> 8) + 1) & Y;
		u8 addr_lo = addr & 0xFF;
		u8 addr_hi = AND;
		WriteCycle(addr_lo | addr_hi << 8, AND);
	}


	// Unofficial instruction; combined ASL and ORA.
	void SLO()
	{
		// ASL
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand << 1;
		WriteCycle(addr, result);
		status.carry = operand & 0x80;
		// ORA
		A |= result;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; combined LSR and EOR.
	void SRE() // LSE
	{
		// LSR
		WriteCycle(addr, operand); /* Dummy write */
		u8 result = operand >> 1;
		WriteCycle(addr, result);
		status.carry = operand & 1;
		// EOR
		A ^= result;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	// Unofficial instruction; STop the Processor.
	void STP()
	{
		stopped = true;
		/* Idle until the end of the "cpu update" */
		while (cpu_cycle_counter < cycle_run_len) {
			WaitCycle();
		}
	}


	// Unofficial instruction; store A AND X in SP and A AND X AND (high byte of addr + 1) at addr.
	void TAS() // XAS, SHS
	{
		sp = A & X;
		u8 op = (addr >> 8) + 1;
		WriteCycle(addr, A & X & op);
	}


	// Unofficial instruction; highly unstable instruction that stores (A OR CONST) AND X AND M into A, where CONST depends on things such as the temperature!
	void XAA()
	{
		// Use CONST = 0
		u8 M = read_addr;
		A &= X & M;
		status.zero = A == 0;
		status.neg = A & 0x80;
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(A);
		stream.StreamPrimitive(X);
		stream.StreamPrimitive(Y);
		stream.StreamPrimitive(sp);
		stream.StreamPrimitive(pc);
		stream.StreamPrimitive(status);

		stream.StreamPrimitive(odd_cpu_cycle);
		stream.StreamPrimitive(stopped);

		stream.StreamPrimitive(nmi_line);
		stream.StreamPrimitive(polled_nmi_line);
		stream.StreamPrimitive(prev_polled_nmi_line);
		stream.StreamPrimitive(need_nmi);
		stream.StreamPrimitive(polled_need_nmi);
		stream.StreamPrimitive(need_irq);
		stream.StreamPrimitive(polled_need_irq);
		stream.StreamPrimitive(write_to_irq_disable_flag_before_next_instr);
		stream.StreamPrimitive(bit_to_write_to_irq_disable_flag);
		stream.StreamPrimitive(irq_line);

		stream.StreamPrimitive(cpu_cycle_counter);
		stream.StreamPrimitive(total_cpu_cycle_counter);
		stream.StreamPrimitive(cpu_cycles_since_reset);
		stream.StreamPrimitive(cpu_cycles_until_all_ppu_regs_writable);
		stream.StreamPrimitive(cpu_cycles_until_no_longer_stalled);

		/* Note: state saving/loading is done after Run() has finished.
			Potential OAMDMA transfers finish before then, so data associated
			to that does not need to be streamed.
			The same applies to the InstrDetails struct; we always stream after an instruction has been executed. */
	}
}