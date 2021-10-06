#pragma once

#include <wx/msgdlg.h>

#include <array>
#include <functional>

#include "../debug/Logging.h"

#include "Bus.h"
#include "Component.h"

// https://www.masswerk.at/6502/6502_instruction_set.html
// http://www.6502.org/tutorials/65c02opcodes.html

class CPU final : public Component
{
public:
	Bus* bus;

	// Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC)
	bool all_ppu_regs_writable = false;

	void Power();
	void Reset();
	void Run();
	void RunInitialCycles();
	void Stall();

	void PollInterruptInputs()
	{
		/* This function is called from within the PPU update function, 2/3 into each cpu cycle.
		   The NMI input is connected to an edge detector, which polls the status of the NMI line during the second half of each cpu cycle.
		   It raises an internal signal if the input goes from being high during one cycle to being low during the next.
		   The internal signal goes high during the first half of the cycle that follows the one where the edge is detected, and stays high until the NMI has been handled.
		   Note: 'need_NMI' is set to true as soon as the internal signal is raised, while 'polled_need_NMI' is updated to 'need_NMI' only one cycle after this. 'need_NMI_polled' is what determines whether to service an NMI after an instruction. */
		prev_polled_NMI_line = polled_NMI_line;
		polled_NMI_line = NMI_line;
		if (polled_NMI_line == 0 && prev_polled_NMI_line == 1)
			need_NMI = true;

		/* The IRQ input is connected to a level detector, which polls the status of the IRQ line during the second half of each cpu cycle.
		   If a low level is detected (at least one bit of IRQ_line is clear), it raises an internal signal during the first half of the next cycle, which remains high for that cycle only. */
		need_IRQ = IRQ_line != 0xFF;
	}
	void PollInterruptOutputs()
	{
		/* This function is called at the start of each CPU cycle.
		   need_NMI/need_IRQ signifies that we need to service an interrupt.
		   However, on real HW, these "signals" are only polled during the last cycle of an instruction. */
		polled_need_NMI = need_NMI;
		polled_need_IRQ = need_IRQ;
	}
	void SetIRQLow(u8 source_mask)  { IRQ_line &= ~source_mask; }
	void SetIRQHigh(u8 source_mask) { IRQ_line |= source_mask; }
	void SetNMILow()  { NMI_line = 0; }
	void SetNMIHigh() { NMI_line = 1; }

	void StartOAMDMATransfer(u8 page, u8* oam_start_ptr, u8 offset);

	void State(Serialization::BaseFunctor& functor) override;

private:
	friend class Logging;

	enum class AddrMode
	{
		Implied,
		Accumulator,
		Immediate,
		Zero_page,
		Zero_page_X,
		Zero_page_Y,
		Absolute,
		Absolute_X,
		Absolute_Y,
		Relative,
		Indirect,
		Indexed_indirect,
		Indirect_indexed
	};

	typedef void (CPU::* instr_t)();
	typedef instr_t addr_mode_fun_t;

	struct InstrDetails // properties of the instruction currently being executed
	{
		u8 opcode;
		instr_t instr;
		AddrMode addr_mode;
		addr_mode_fun_t addr_mode_fun;

		bool page_crossing_possible;
		bool page_crossed;

		u8 read_addr;
		u16 addr;
	} curr_instr;

	static const size_t num_instr = 0x100;

	const instr_t instr_table[num_instr] =
	{// $x0 / $x8  $x1 / $x9  $x2 / $xA  $x3 / $xB  $x4 / $xC  $x5 / $xD  $x6 / $xE  $x7 / $xF
		&CPU::BRK, &CPU::ORA, &CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, // $0x
		&CPU::PHP, &CPU::ORA, &CPU::ASL, &CPU::ANC, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, // $0x
		&CPU::BPL, &CPU::ORA, &CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, // $1x
		&CPU::CLC, &CPU::ORA, &CPU::NOP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, // $1x
		&CPU::JSR, &CPU::AND, &CPU::STP, &CPU::RLA, &CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA, // $2x
		&CPU::PLP, &CPU::AND, &CPU::ROL, &CPU::ANC, &CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA, // $2x
		&CPU::BMI, &CPU::AND, &CPU::STP, &CPU::RLA, &CPU::NOP, &CPU::AND, &CPU::ROL, &CPU::RLA, // $3x
		&CPU::SEC, &CPU::AND, &CPU::NOP, &CPU::RLA, &CPU::NOP, &CPU::AND, &CPU::ROL, &CPU::RLA, // $3x

		&CPU::RTI, &CPU::EOR, &CPU::STP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, // $4x
		&CPU::PHA, &CPU::EOR, &CPU::LSR, &CPU::ALR, &CPU::JMP, &CPU::EOR, &CPU::LSR, &CPU::SRE, // $4x
		&CPU::BVC, &CPU::EOR, &CPU::STP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, // $5x
		&CPU::CLI, &CPU::EOR, &CPU::NOP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, // $5x
		&CPU::RTS, &CPU::ADC, &CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA, // $6x
		&CPU::PLA, &CPU::ADC, &CPU::ROR, &CPU::ARR, &CPU::JMP, &CPU::ADC, &CPU::ROR, &CPU::RRA, // $6x
		&CPU::BVS, &CPU::ADC, &CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA, // $7x
		&CPU::SEI, &CPU::ADC, &CPU::NOP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA, // $7x

		&CPU::NOP, &CPU::STA, &CPU::NOP, &CPU::SAX, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, // $8x
		&CPU::DEY, &CPU::NOP, &CPU::TXA, &CPU::XAA, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, // $8x
		&CPU::BCC, &CPU::STA, &CPU::STP, &CPU::AHX, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, // $9x
		&CPU::TYA, &CPU::STA, &CPU::TXS, &CPU::TAS, &CPU::SHY, &CPU::STA, &CPU::SHX, &CPU::AHX, // $9x
		&CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, // $Ax
		&CPU::TAY, &CPU::LDA, &CPU::TAX, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, // $Ax
		&CPU::BCS, &CPU::LDA, &CPU::STP, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, // $Bx
		&CPU::CLV, &CPU::LDA, &CPU::TSX, &CPU::LAS, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, // $Bx

		&CPU::CPY, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::CPY, &CPU::CMP, &CPU::DEC, &CPU::DCP, // $Cx
		&CPU::INY, &CPU::CMP, &CPU::DEX, &CPU::AXS, &CPU::CPY, &CPU::CMP, &CPU::DEC, &CPU::DCP, // $Cx
		&CPU::BNE, &CPU::CMP, &CPU::STP, &CPU::DCP, &CPU::NOP, &CPU::CMP, &CPU::DEC, &CPU::DCP, // $Dx
		&CPU::CLD, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::NOP, &CPU::CMP, &CPU::DEC, &CPU::DCP, // $Dx
		&CPU::CPX, &CPU::SBC, &CPU::NOP, &CPU::ISC, &CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC, // $Ex
		&CPU::INX, &CPU::SBC, &CPU::NOP, &CPU::SBC, &CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC, // $Ex
		&CPU::BEQ, &CPU::SBC, &CPU::STP, &CPU::ISC, &CPU::NOP, &CPU::SBC, &CPU::INC, &CPU::ISC, // $Fx
		&CPU::SED, &CPU::SBC, &CPU::NOP, &CPU::ISC, &CPU::NOP, &CPU::SBC, &CPU::INC, &CPU::ISC  // $Fx
	};

	const addr_mode_fun_t addr_mode_fun_table[13] =
	{
		&CPU::ExecImplied, &CPU::ExecAccumulator, &CPU::ExecImmediate, &CPU::ExecZeroPage, &CPU::ExecZeroPageX,
		&CPU::ExecZeroPageY, &CPU::ExecAbsolute, &CPU::ExecAbsoluteX, &CPU::ExecAbsoluteY, &CPU::ExecRelative,
		&CPU::ExecIndirect, &CPU::ExecIndexedIndirect, &CPU::ExecIndirectIndexed
	};

	u8 A, X, Y; // registers
	u8 SP; // stack pointer
	u16 PC; // program counter

	struct Flags { bool N, V, B, D, I, Z, C; } flags;

	bool odd_cpu_cycle;
	bool stalled; // set to true by the APU when the DMC memory reader reads a byte from PRG.
	bool stopped; // set to true by the STP instruction

	// Interrupt-related
	enum class InterruptType { NMI, IRQ, BRK };
	bool NMI_line = 1; // The NMI signal coming from the ppu.
	bool polled_NMI_line = 1, prev_polled_NMI_line = 1; // The polled NMI line signal during the 2nd half of the last and second to last CPU cycle, respectively.
	bool need_NMI = false; // Whether we need to service an NMI interrupt. Is set right after a negative edge is detected (prev_polled_NMI_line == 1 && polled_NMI_line == 0)
	bool polled_need_NMI = false; // Same as above, but this only gets updated to 'need_NMI' only cycle after need_NMI is updated. If this is set after we have executed an instruction, we service the NMI.
	bool need_IRQ = false;
	bool polled_need_IRQ = false;
	bool write_to_interrupt_disable_flag_on_next_cycle = false;
	bool bit_to_write_to_interrupt_disable_flag;
	u8 IRQ_line = 0xFF; // One bit for each IRQ source (there are eight according to https://wiki.nesdev.org/w/index.php?title=IRQ)

	// OAMDMA-related
	bool oam_dma_transfer_pending;
	u8* oam_dma_base_write_addr_ptr;
	u8 oam_dma_write_addr_offset;
	u16 oam_dma_base_read_addr;
	void PerformOAMDMATransfer();

	unsigned cpu_cycle_counter; /* Cycles elapsed during the current call to Update(). */
	unsigned total_cpu_cycle_counter = 0; /* Cycles elapsed since the game was started. */

	unsigned cpu_cycles_since_reset = 0; /* Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC) */
	unsigned cpu_cycles_until_all_ppu_regs_writable = 29658;
	unsigned cpu_cycles_until_no_longer_stalled; // refers to stalling done by the APU when the DMC memory reader reads a byte from PRG

	AddrMode GetAddressingModeFromOpcode(u8 opcode) const;

	void ExecuteInstruction();

	void ExecImplied();
	void ExecAccumulator();
	void ExecImmediate();
	void ExecZeroPage();
	void ExecZeroPageX() { ExecZeroPageIndexed(X); }
	void ExecZeroPageY() { ExecZeroPageIndexed(Y); }
	void ExecZeroPageIndexed(u8& index_reg);
	void ExecAbsolute();
	void ExecAbsoluteX() { ExecAbsoluteIndexed(X); }
	void ExecAbsoluteY() { ExecAbsoluteIndexed(Y); }
	void ExecAbsoluteIndexed(u8& index_reg);
	void ExecRelative();
	void ExecIndirect();
	void ExecIndexedIndirect();
	void ExecIndirectIndexed();

	void ServiceInterrupt(InterruptType asserted_interrupt_type);

	// official instructions
	void ADC();
	void AND();
	void ASL();
	void BCC();
	void BCS();
	void BEQ();
	void BIT();
	void BMI();
	void BNE();
	void BPL();
	void BRK();
	void BVC();
	void BVS();
	void CLC();
	void CLD();
	void CLI();
	void CLV();
	void CMP();
	void CPX();
	void CPY();
	void DEC();
	void DEX();
	void DEY();
	void EOR();
	void INC();
	void INX();
	void INY();
	void JMP();
	void JSR();
	void LDA();
	void LDX();
	void LDY();
	void LSR();
	void NOP();
	void ORA();
	void PHA();
	void PHP();
	void PLA();
	void PLP();
	void ROL();
	void ROR();
	void RTI();
	void RTS();
	void SBC();
	void SEC();
	void SED();
	void SEI();
	void STA();
	void STX();
	void STY();
	void TAX();
	void TAY();
	void TSX();
	void TXA();
	void TXS();
	void TYA();

	// unoffical instructions
	void AHX();
	void ALR();
	void ANC();
	void ARR();
	void AXS();
	void DCP();
	void ISC();
	void LAS();
	void LAX();
	void RLA();
	void RRA();
	void SAX();
	void SHX();
	void SHY();
	void SLO();
	void SRE();
	void STP();
	void TAS();
	void XAA();

	// Helper functions. Defined in the header to enable inlining
	__forceinline void StartCycle()
	{
#ifdef DEBUG
		total_cpu_cycle_counter++;
#endif
		cpu_cycle_counter++;
		odd_cpu_cycle = !odd_cpu_cycle;
		PollInterruptOutputs();
	}

	__forceinline u8 ReadCycle(u16 addr)
	{
		StartCycle();
		return bus->ReadCycle(addr);
	}

	__forceinline void WriteCycle(u16 addr, u8 data)
	{
		StartCycle();
		bus->WriteCycle(addr, data);
	}

	__forceinline void WaitCycle()
	{
		StartCycle();
		bus->WaitCycle();
	}

	__forceinline void PushByteToStack(u8 byte)
	{
		WriteCycle(0x0100 | SP--, byte);
	}

	__forceinline void PushWordToStack(u16 word)
	{
		WriteCycle(0x0100 | SP--, word >> 8);
		WriteCycle(0x0100 | SP--, word & 0xFF);
	}

	__forceinline u8 PullByteFromStack()
	{
		return ReadCycle(0x0100 | ++SP);
	}

	__forceinline u16 PullWordFromStack()
	{
		u8 lo = ReadCycle(0x0100 | ++SP);
		u8 hi = ReadCycle(0x0100 | ++SP);
		return hi << 8 | lo;
	}

	__forceinline u16 ReadWord(u16 addr)
	{
		u8 lo = ReadCycle(addr);
		u8 hi = ReadCycle(addr + 1);
		return lo | hi << 8;
	}

	__forceinline void Branch(bool cond)
	{
		if (cond)
		{
			s8 offset = curr_instr.addr;
			// +1 cycle if branch succeeds, +2 if to a new page
			WaitCycle();
			if ((PC & 0xFF00) != ((u16)(PC + offset) & 0xFF00))
				WaitCycle();
			PC += offset;
		}
	}

	// called when an instruction wants access to the status register
	u8 GetStatusRegInstr(instr_t instr) const
	{
		bool bit4 = (instr == &CPU::BRK || instr == &CPU::PHP);
		return flags.N << 7 | flags.V << 6 | 1 << 5 | bit4 << 4 | flags.D << 3 | flags.I << 2 | flags.Z << 1 | flags.C;
	}

	// called when an interrupt is being serviced and the status register is pushed to the stack
	u8 GetStatusRegInterrupt() const
	{
		return flags.N << 7 | flags.V << 6 | 1 << 5 | flags.D << 3 | flags.I << 2 | flags.Z << 1 | flags.C;
	}

	void SetStatusReg(u8 value)
	{
		flags.N = value & 0x80;
		flags.V = value & 0x40;
		flags.B = value & 0x10;
		flags.D = value & 0x08;
		flags.I = value & 0x04;
		flags.Z = value & 0x02;
		flags.C = value & 0x01;
	}

	u8 GetReadInstrOperand()
	{
		if (curr_instr.addr_mode == AddrMode::Immediate)
			return ReadCycle(PC++);
		if (curr_instr.page_crossing_possible && !curr_instr.page_crossed) // Possible only if Absolute Indexed or Indirect Indexed addressing is used
			return curr_instr.read_addr;
		return ReadCycle(curr_instr.addr);
	}

	/// Debugging-related
	enum class Action { Instruction, NMI };
	void LogState(Action action);
};

