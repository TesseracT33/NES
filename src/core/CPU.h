#pragma once

#include <wx/msgdlg.h>

#include <array>
#include <functional>

#include "Bus.h"
#include "Component.h"

//#define DEBUG_LOG
#define DEBUG_LOG_PATH "F:\\nes_cpu_debug.txt"

#define DEBUG_COMPARE_NESTEST
#define NESTEST_LOG_PATH "C:\\Users\\Christoffer\\source\\repos\\games\\nes\\nestest.log"

#define DEBUG (DEBUG_LOG || DEBUG_COMPARE_NESTEST)

class CPU final : public Component
{
public:
	#ifdef DEBUG
		unsigned instruction_counter = 1;
		unsigned cpu_cycle_counter = 6;
	#endif

	CPU();

	Bus* bus;

	// Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC)
	bool all_ppu_regs_writable = false;

	void Reset();
	void Power();
	void Update();

	void SetIRQLow();
	void SetIRQHigh();
	void SetNMILow();
	void SetNMIHigh();

	void IncrementCycleCounter();
	void Set_OAM_DMA_Active();

	void State(Serialization::BaseFunctor& functor) override;

private:
	enum class AddrMode
	{
		Implicit,
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

	enum InstrType
	{
		Implicit,
		Read,
		Write,
		Read_modify_write
	};

	typedef void (CPU::* instr_t)();
	typedef instr_t addr_mode_fun_t;

	struct InstrDetails // properties of the instruction currently being executed
	{
		bool instr_executing;
		u8 opcode;
		instr_t instr;
		AddrMode addr_mode;
		addr_mode_fun_t addr_mode_fun;
		InstrType instr_type;
		int cycle;
		bool addition_overflow;

		u8 addr_lo, addr_hi;
		u8 read_addr;
		u16 addr;

		u8 new_target;
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
		&CPU::StepImplicit, &CPU::StepAccumulator, &CPU::StepImmediate, &CPU::StepZeroPage, &CPU::StepZeroPageX,
		&CPU::StepZeroPageY, &CPU::StepAbsolute, &CPU::StepAbsoluteX, &CPU::StepAbsoluteY, &CPU::StepRelative,
		&CPU::StepIndirect, &CPU::StepIndexedIndirect, &CPU::StepIndirectIndexed
	};

	u8 A, X, Y; // registers
	u8 S; // stack pointer
	u16 PC; // program counter

	struct Flags { bool N, V, B, D, I, Z, C; } flags;

	bool odd_cpu_cycle;
	bool stopped; // set to true by the STP instruction

	// interrupt-related
	enum class InterruptType { NMI, IRQ, BRK };
	InterruptType handled_interrupt_type; // the type that was handled during the last interrupt servicing (different from the 'asserted' type; see ServiceInterrupt())
	bool NMI_signal_active = false;
	bool clear_I_on_next_update = false, set_I_on_next_update = false; // refers to the I flag in the status register
	unsigned IRQ_num_inputs = 0; // how many devices are currently pulling the IRQ signal down

	bool oam_dma_transfer_active = false;
	unsigned oam_dma_cycles_until_finished;

	// Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC)
	unsigned cpu_clocks_since_reset = 0;
	unsigned cpu_clocks_until_all_ppu_regs_writable = 29658;

	// Maps opcodes to instruction types (Read, write, etc.). 
	// Built in the ctor of CPU. Making it constexpr did not work smoothly
	std::array<InstrType, num_instr> instr_type_table;

	AddrMode GetAddressingModeFromOpcode(u8 opcode) const;

	void BeginInstruction();

	void StepImplicit();
	void StepAccumulator();
	void StepImmediate();
	void StepZeroPage();
	void StepZeroPageX() { StepZeroPageIndexed(X); }
	void StepZeroPageY() { StepZeroPageIndexed(Y); }
	void StepZeroPageIndexed(u8& index_reg);
	void StepAbsolute();
	void StepAbsoluteX() { StepAbsoluteIndexed(X); }
	void StepAbsoluteY() { StepAbsoluteIndexed(Y); }
	void StepAbsoluteIndexed(u8& index_reg);
	void StepRelative();
	void StepIndirect();
	void StepIndexedIndirect();
	void StepIndirectIndexed();

	void ServiceInterrupt(InterruptType asserted_interrupt_type);

	void BuildInstrTypeTable();

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
	void PushByteToStack(u8 byte)
	{
		bus->Write(0x0100 | S--, byte);
	}

	void PushWordToStack(u16 word)
	{
		bus->Write(0x0100 | S--, word >> 8);
		bus->Write(0x0100 | S--, word & 0xFF);
	}

	u8 PullByteFromStack()
	{
		return bus->Read(0x0100 | ++S);
	}

	u16 PullWordFromStack()
	{
		u8 lo = bus->Read(0x0100 | ++S);
		u8 hi = bus->Read(0x0100 | ++S);
		return hi << 8 | lo;
	}

	void Branch(bool cond)
	{
		if (cond)
		{
			s8 offset = (s8)curr_instr.addr_lo;
			// +1 cycle if branch succeeds, +2 if to a new page
			unsigned additional_cycles = (PC & 0xFF00) == ((u16)(PC + offset) & 0xFF00) ? 1 : 2;
			bus->WaitCycle(additional_cycles);
			PC += offset;
		}
	}

	u8 Read_u8()
	{
		return bus->Read(PC++);
	}

	s8 Read_s8()
	{
		return (s8)bus->Read(PC++);
	}

	u16 Read_u16()
	{
		u8 lo = bus->Read(PC++);
		u8 hi = bus->Read(PC++);
		return hi << 8 | lo;
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

	////// Debugging-related //////
#ifdef DEBUG_LOG
	void Log_PrintLine();
#endif

#ifdef DEBUG_COMPARE_NESTEST
	void NesTest_Compare();

	struct NesTest
	{
		std::string current_line;
		unsigned line_counter = 0;
	} nestest;
#endif
};

