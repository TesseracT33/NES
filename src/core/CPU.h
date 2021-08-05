#pragma once

#include <array>
#include <functional>

#include "Bus.h"

#define DEBUG

class CPU final : public Component
{
public:
	CPU();

	Bus* bus;

	void BeginInstruction();
	void Initialize() override;
	void Reset() override;
	void Update() override;

	void   Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

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

	u8 A, X, Y; // registers
	u8 S; // stack pointer
	u16 PC; // program counter

	struct Flags { bool C, Z, I, D, V, N, B; } flags;

	const instr_t instr_table[num_instr] =
	{
		&CPU::BRK, &CPU::ORA, &CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, 
		&CPU::PHP, &CPU::ORA, &CPU::ASL, &CPU::ANC, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO,
		&CPU::BPL, &CPU::ORA, &CPU::STP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO, 
		&CPU::CLC, &CPU::ORA, &CPU::NOP, &CPU::SLO, &CPU::NOP, &CPU::ORA, &CPU::ASL, &CPU::SLO,
		&CPU::JSR, &CPU::AND, &CPU::STP, &CPU::RLA, &CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA, 
		&CPU::PLP, &CPU::AND, &CPU::ROL, &CPU::ANC, &CPU::BIT, &CPU::AND, &CPU::ROL, &CPU::RLA,
		&CPU::BMI, &CPU::AND, &CPU::STP, &CPU::RLA, &CPU::NOP, &CPU::AND, &CPU::ROL, &CPU::RLA, 
		&CPU::SEC, &CPU::AND, &CPU::NOP, &CPU::RLA, &CPU::NOP, &CPU::AND, &CPU::ROL, &CPU::RLA,

		&CPU::RTI, &CPU::EOR, &CPU::STP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, 
		&CPU::PHA, &CPU::EOR, &CPU::LSR, &CPU::ALR, &CPU::JMP, &CPU::EOR, &CPU::LSR, &CPU::SRE,
		&CPU::BVC, &CPU::EOR, &CPU::STP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE, 
		&CPU::CLI, &CPU::EOR, &CPU::NOP, &CPU::SRE, &CPU::NOP, &CPU::EOR, &CPU::LSR, &CPU::SRE,
		&CPU::RTS, &CPU::ADC, &CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA, 
		&CPU::PLA, &CPU::ADC, &CPU::ROR, &CPU::ARR, &CPU::JMP, &CPU::ADC, &CPU::ROR, &CPU::RRA,
		&CPU::BVS, &CPU::ADC, &CPU::STP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA, 
		&CPU::SEI, &CPU::ADC, &CPU::NOP, &CPU::RRA, &CPU::NOP, &CPU::ADC, &CPU::ROR, &CPU::RRA,

		&CPU::NOP, &CPU::STA, &CPU::NOP, &CPU::SAX, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, 
		&CPU::DEY, &CPU::NOP, &CPU::TXA, &CPU::XAA, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX,
		&CPU::BCC, &CPU::STA, &CPU::STP, &CPU::AHX, &CPU::STY, &CPU::STA, &CPU::STX, &CPU::SAX, 
		&CPU::TYA, &CPU::STA, &CPU::TXS, &CPU::TAS, &CPU::SHY, &CPU::STA, &CPU::SHX, &CPU::AHX,
		&CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, 
		&CPU::TAY, &CPU::LDA, &CPU::TAX, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX,
		&CPU::BCS, &CPU::LDA, &CPU::STP, &CPU::LAX, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX, 
		&CPU::CLV, &CPU::LDA, &CPU::TSX, &CPU::LAS, &CPU::LDY, &CPU::LDA, &CPU::LDX, &CPU::LAX,

		&CPU::CPY, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::CPY, &CPU::CMP, &CPU::DEC, &CPU::DCP, 
		&CPU::INY, &CPU::CMP, &CPU::DEX, &CPU::AXS, &CPU::CPY, &CPU::CMP, &CPU::DEC, &CPU::DCP,
		&CPU::BNE, &CPU::CMP, &CPU::STP, &CPU::DCP, &CPU::NOP, &CPU::CMP, &CPU::DEC, &CPU::DCP, 
		&CPU::DLC, &CPU::CMP, &CPU::NOP, &CPU::DCP, &CPU::NOP, &CPU::CMP, &CPU::DEC, &CPU::DCP,
		&CPU::CPX, &CPU::SBC, &CPU::NOP, &CPU::ISC, &CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC, 
		&CPU::INX, &CPU::SBC, &CPU::NOP, &CPU::SBC, &CPU::CPX, &CPU::SBC, &CPU::INC, &CPU::ISC,
		&CPU::BEQ, &CPU::SBC, &CPU::STP, &CPU::ISC, &CPU::NOP, &CPU::SBC, &CPU::INC, &CPU::ISC, 
		&CPU::SED, &CPU::SBC, &CPU::NOP, &CPU::ISC, &CPU::NOP, &CPU::SBC, &CPU::INC, &CPU::ISC
	};


	const addr_mode_fun_t addr_mode_fun_table[13] =
	{
		&CPU::StepImplicit, &CPU::StepAccumulator, &CPU::StepImmediate, &CPU::StepZeroPage, &CPU::StepZeroPageX,
		&CPU::StepZeroPageY, &CPU::StepAbsolute, &CPU::StepAbsoluteX, &CPU::StepAbsoluteY, &CPU::StepRelative,
		&CPU::StepIndirect, &CPU::StepIndexedIndirect, &CPU::StepIndirectIndexed
	};

	AddrMode DetermineAddressingMode(u8 opcode) const;

	// Maps opcodes to instruction types (Read, write, etc.). 
	// Built in the ctor of CPU. Making it constexpr did not work smoothly
	std::array<InstrType, num_instr> instr_type_table;


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
	void DLC();
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
		bus->Write(0x0100 | --S, byte);
	}

	void PushWordToStack(u16 word)
	{
		bus->Write(0x0100 | --S, word >> 8);
		bus->Write(0x0100 | --S, word & 0xFF);
	}

	u8 PullByteFromStack()
	{
		return bus->Read(0x0100 | S++);
	}

	u16 PullWordFromStack()
	{
		u8 lo = bus->Read(0x0100 | S++);
		u8 hi = bus->Read(0x0100 | S++);
		return hi << 8 | lo;
	}

	void Branch(bool cond)
	{
		// todo: read offset even if !cond?
		if (cond)
		{
			s8 offset = Read_s8();
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

	u8 GetStatusReg(instr_t instr) const
	{
		bool bit4 = (instr == &CPU::BRK || instr == &CPU::PHP);
		return flags.N << 7 | flags.V << 6 | 1 << 5 | bit4 << 4 | flags.D << 3 | flags.I << 2 | flags.Z << 1 | flags.C;
	}
};

