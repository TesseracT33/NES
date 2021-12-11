#pragma once

#include <array>
#include <functional>

#include "../debug/Logging.h"

#include "Bus.h"
#include "Component.h"
#include "IRQSources.h"

class CPU final : public Component
{
public:
	using Component::Component;

	// Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC)
	bool all_ppu_regs_writable = false;

	void PowerOn();
	void Reset(bool jump_to_reset_vector = true);
	void Run();
	void RunStartUpCycles();
	void Stall();

	__forceinline void PollInterruptInputs()
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
	__forceinline void PollInterruptOutputs()
	{
		/* This function is called at the start of each CPU cycle.
		   need_NMI/need_IRQ signifies that we need to service an interrupt.
		   However, on real HW, these "signals" are only polled during the last cycle of an instruction. */
		polled_need_NMI = need_NMI;
		polled_need_IRQ = need_IRQ;
	}
	__forceinline void SetIRQLow(IRQSource source_mask)  { IRQ_line &= ~static_cast<u8>(source_mask); }
	__forceinline void SetIRQHigh(IRQSource source_mask) { IRQ_line |=  static_cast<u8>(source_mask); }
	__forceinline void SetNMILow()  { NMI_line = 0; }
	__forceinline void SetNMIHigh() { NMI_line = 1; }

	void StartOAMDMATransfer(u8 page, u8* oam_start_ptr, u8 offset);

	void StreamState(SerializationStream& stream) override;

private:
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

	enum class InterruptType { NMI, IRQ, BRK };

	typedef void (CPU::* instr_t)();
	typedef instr_t addr_mode_fun_t;

	struct InstrDetails // Details/properties of the instruction currently being executed
	{
		u8 opcode;
		instr_t instr;
		AddrMode addr_mode;
		addr_mode_fun_t addr_mode_fun;

		bool page_crossed;

		u8 read_addr;
		u16 addr;
	} curr_instr;

	static const size_t num_opcodes = 0x100;

	/* Maps opcodes to instructions */
	const instr_t instr_table[num_opcodes] =
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

	/* Maps opcodes to addressing modes */
	static constexpr std::array<CPU::AddrMode, num_opcodes> addr_mode_table = []
	{
		std::array<CPU::AddrMode, num_opcodes> table{};
		for (size_t opcode = 0x00; opcode < num_opcodes; opcode++)
		{
			switch (opcode & 0x1F)
			{
			case 0x00:
				if (opcode == 0x20)                table[opcode] = AddrMode::Absolute;
				else if ((opcode & ~0x1F) >= 0x80) table[opcode] = AddrMode::Immediate;
				else                               table[opcode] = AddrMode::Implied;
				break;
			case 0x01: table[opcode] = AddrMode::Indexed_indirect; break;
			case 0x02: table[opcode] = (opcode & ~0x1F) >= 0x80 ? AddrMode::Immediate : AddrMode::Implied; break;
			case 0x03: table[opcode] = AddrMode::Indexed_indirect; break;
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07: table[opcode] = AddrMode::Zero_page; break;
			case 0x08: table[opcode] = AddrMode::Implied; break;
			case 0x09: table[opcode] = AddrMode::Immediate; break;
			case 0x0A: table[opcode] = (opcode & ~0x1F) >= 0x80 ? AddrMode::Implied : AddrMode::Accumulator; break;
			case 0x0B: table[opcode] = AddrMode::Immediate; break;
			case 0x0C: table[opcode] = (opcode == 0x6C) ? AddrMode::Indirect : AddrMode::Absolute; break;
			case 0x0D:
			case 0x0E:
			case 0x0F: table[opcode] = AddrMode::Absolute; break;
			case 0x10: table[opcode] = AddrMode::Relative; break;
			case 0x11: table[opcode] = AddrMode::Indirect_indexed; break;
			case 0x12: table[opcode] = AddrMode::Implied; break;
			case 0x13: table[opcode] = AddrMode::Indirect_indexed; break;
			case 0x14:
			case 0x15: table[opcode] = AddrMode::Zero_page_X; break;
			case 0x16:
			case 0x17: table[opcode] = (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Zero_page_Y : AddrMode::Zero_page_X; break;
			case 0x18: table[opcode] = AddrMode::Implied; break;
			case 0x19: table[opcode] = AddrMode::Absolute_Y; break;
			case 0x1A: table[opcode] = AddrMode::Implied; break;
			case 0x1B: table[opcode] = AddrMode::Absolute_Y; break;
			case 0x1C:
			case 0x1D: table[opcode] = AddrMode::Absolute_X; break;
			case 0x1E:
			case 0x1F: table[opcode] = (opcode & ~0x1F) == 0x80 || (opcode & ~0x1F) == 0xA0 ? AddrMode::Absolute_Y : AddrMode::Absolute_X; break;
			}
		}
		return table;
	}();

	/* Maps addressing modes to functions to be executed for the given addressing mode */
	const addr_mode_fun_t addr_mode_fun_table[13] =
	{
		&CPU::ExecImplied, &CPU::ExecAccumulator, &CPU::ExecImmediate, &CPU::ExecZeroPage, &CPU::ExecZeroPageX,
		&CPU::ExecZeroPageY, &CPU::ExecAbsolute, &CPU::ExecAbsoluteX, &CPU::ExecAbsoluteY, &CPU::ExecRelative,
		&CPU::ExecIndirect, &CPU::ExecIndexedIndirect, &CPU::ExecIndirectIndexed
	};

	u8 A, X, Y; // general-purpose registers
	u8 SP; // stack pointer
	u16 PC; // program counter

	/* Status flags, encoded in the processor status register (P)
	  7  bit  0
	  ---- ----
	  NV1B DIZC
	  |||| ||||
	  |||| |||+- Carry
	  |||| ||+-- Zero
	  |||| |+--- Interrupt Disable
	  |||| +---- Decimal
	  |||+------ The B flag; no CPU effect. Reads either 0 or 1 in different situations.
	  ||+------- Always reads 1.
	  |+-------- Overflow
	  +--------- Negative
	*/
	struct Flags { bool N, V, B, D, I, Z, C; } flags;

	bool odd_cpu_cycle; // if the current cpu cycle is odd_numbered
	bool stalled; // set to true by the APU when the DMC memory reader reads a byte from PRG.
	bool stopped; // set to true by the STP instruction

	// Interrupt-related
	bool NMI_line; // The NMI signal coming from the ppu.
	bool polled_NMI_line, prev_polled_NMI_line; // The polled NMI line signal during the 2nd half of the last and second to last CPU cycle, respectively.
	bool need_NMI; // Whether we need to service an NMI interrupt. Is set right after a negative edge is detected (prev_polled_NMI_line == 1 && polled_NMI_line == 0)
	bool polled_need_NMI; // Same as above, but this only gets updated to 'need_NMI' only cycle after need_NMI is updated. If this is set after we have executed an instruction, we service the NMI.
	bool need_IRQ;
	bool polled_need_IRQ;
	bool write_to_interrupt_disable_flag_before_next_instr;
	bool bit_to_write_to_interrupt_disable_flag;
	u8 IRQ_line; // One bit for each IRQ source (there are eight according to https://wiki.nesdev.org/w/index.php?title=IRQ)

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

	// "unoffical" instructions; these are not used by the vast majority of games
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

	// Helper functions
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
		return nes->bus->ReadCycle(addr);
	}

	__forceinline void WriteCycle(u16 addr, u8 data)
	{
		StartCycle();
		nes->bus->WriteCycle(addr, data);
	}

	__forceinline void WaitCycle()
	{
		StartCycle();
		nes->bus->WaitCycle();
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
		return hi << 8 | lo;
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
	__forceinline u8 GetStatusRegInstr(instr_t instr) const
	{
		bool bit4 = (instr == &CPU::BRK || instr == &CPU::PHP);
		return flags.N << 7 | flags.V << 6 | 1 << 5 | bit4 << 4 | flags.D << 3 | flags.I << 2 | flags.Z << 1 | flags.C;
	}

	// called when an interrupt is being serviced and the status register is pushed to the stack
	__forceinline u8 GetStatusRegInterrupt() const
	{
		return flags.N << 7 | flags.V << 6 | 1 << 5 | flags.D << 3 | flags.I << 2 | flags.Z << 1 | flags.C;
	}

	__forceinline void SetStatusReg(u8 value)
	{
		flags.N = value & 0x80;
		flags.V = value & 0x40;
		flags.B = value & 0x10;
		flags.D = value & 0x08;
		flags.I = value & 0x04;
		flags.Z = value & 0x02;
		flags.C = value & 0x01;
	}

	__forceinline u8 GetReadInstrOperand()
	{
		switch (curr_instr.addr_mode)
		{
		case AddrMode::Zero_page:
		case AddrMode::Zero_page_X:
		case AddrMode::Zero_page_Y:
		case AddrMode::Absolute:
		case AddrMode::Indexed_indirect:
			return ReadCycle(curr_instr.addr);

		/* If overflow did not occur when fetching the address, the operand has already been fetched from this address.
		   If not, we need to perform the memory read. */
		case AddrMode::Absolute_X:
		case AddrMode::Absolute_Y:
		case AddrMode::Indirect_indexed:
			if (curr_instr.page_crossed)
				return ReadCycle(curr_instr.addr);
			return curr_instr.read_addr;

		/* For e.g. immediate adressing, the operand has already been read. */
		default:
			return curr_instr.read_addr;
		}
	}

	__forceinline u8 GetReadModWriteInstrOperand()
	{
		/* For these instructions, no matter if a page was crossed when fetching the address, the operand is yet to be read.
		Note: these instructions do not use immediate addressing. */
		return ReadCycle(curr_instr.addr);
	}

	/// Debugging-related
	enum class Action { Instruction, NMI, IRQ };
	void LogStateBeforeAction(Action action);
};

