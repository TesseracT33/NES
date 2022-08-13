export module CPU;

import NumericalTypes;
import SerializationStream;
import Util;

import <array>;
import <utility>;

namespace CPU
{
	export
	{
		enum class InterruptType {
			BRK, IRQ, NMI
		};

		enum class IrqSource : u8 {
			ApuDmc = 1 << 0,
			ApuFrame = 1 << 1,
			MMC3 = 1 << 2,
			MMC5 = 1 << 3,
			VRC = 1 << 4,
			FME7 = 1 << 5,
			NAMCO163 = 1 << 6,
			DF5 = 1 << 7
		};

		void PollInterruptInputs();
		void PowerOn();
		void Reset(bool jump_to_reset_vector = true);
		void Run();
		void RunStartUpCycles();
		void SetIrqHigh(IrqSource source_mask);
		void SetIrqLow(IrqSource source_mask);
		void SetNmiHigh();
		void SetNmiLow();
		void Stall();
		void PerformOamDmaTransfer(u8 page, u8* oam_start_ptr, u8 offset);
		void StreamState(SerializationStream& stream);
	}

	enum class AddrMode {
		Absolute,
		AbsoluteX,
		AbsoluteY,
		Accumulator,
		Implied,
		Immediate,
		IndexedIndirect,
		Indirect,
		IndirectIndexed,
		Relative,
		ZeroPage,
		ZeroPageX,
		ZeroPageY
	};

	using Instruction = void(*)();

	template<Instruction, AddrMode>
	void ExecuteInstruction();

	template<Instruction>
	u8 GetStatusRegInstr();

	template<InterruptType>
	void ServiceInterrupt();

	void Branch(bool cond);
	void FetchDecodeExecuteInstruction();
	u8 GetStatusRegInterrupt();
	void PollInterruptOutputs();
	u8 PullByteFromStack();
	u16 PullWordFromStack();
	void PushByteToStack(u8 byte);
	void PushWordToStack(u16 word);
	u8 ReadCycle(u16 addr);
	u16 ReadWord(u16 addr);
	void SetStatusReg(u8 value);
	void StartCycle();
	void WaitCycle();
	void WriteCycle(u16 addr, u8 data);

	// official instructions
	void ADC();
	void AND();
	void ASL_A();
	void ASL_M();
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
	void LSR_A();
	void LSR_M();
	void NOP();
	void ORA();
	void PHA();
	void PHP();
	void PLA();
	void PLP();
	void ROL_A();
	void ROL_M();
	void ROR_A();
	void ROR_M();
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

	/* How many cycles to run the CPU for each time function Run is called. A frame is roughly 30,000 cpu cycles.
	   Afterwards, input is polled. */
	constexpr uint cycle_run_len = 10000;

	/* Mark this as constexpr and MSVC will freak out */
	 std::array instr_table = {
#define OP(INSTR, ADDR_MODE) ExecuteInstruction<INSTR, AddrMode::ADDR_MODE>
#define ABS Absolute
#define ABX AbsoluteX
#define ABY AbsoluteY
#define ACC Accumulator
#define IMM Immediate
#define IMP Implied
#define IXI IndexedIndirect
#define IND Indirect
#define IIX IndirectIndexed
#define REL Relative
#define ZP ZeroPage
#define ZPX ZeroPageX
#define ZPY ZeroPageY
		OP(BRK,IMP), OP(ORA,IXI), OP(STP  ,IMP), OP(SLO,IXI), OP(NOP,ZP) , OP(ORA,ZP) , OP(ASL_M,ZP) , OP(SLO,ZP) , // $0x
		OP(PHP,IMP), OP(ORA,IMM), OP(ASL_A,ACC), OP(ANC,IMM), OP(NOP,ABS), OP(ORA,ABS), OP(ASL_M,ABS), OP(SLO,ABS), // $0x
		OP(BPL,REL), OP(ORA,IIX), OP(STP  ,IMP), OP(SLO,IIX), OP(NOP,ZPX), OP(ORA,ZPX), OP(ASL_M,ZPX), OP(SLO,ZPX), // $1x
		OP(CLC,IMP), OP(ORA,ABY), OP(NOP  ,IMP), OP(SLO,ABY), OP(NOP,ABX), OP(ORA,ABX), OP(ASL_M,ABX), OP(SLO,ABX), // $1x
		OP(JSR,ABS), OP(AND,IXI), OP(STP  ,IMP), OP(RLA,IXI), OP(BIT,ZP) , OP(AND,ZP) , OP(ROL_M,ZP) , OP(RLA,ZP) , // $2x
		OP(PLP,IMP), OP(AND,IMM), OP(ROL_A,ACC), OP(ANC,IMM), OP(BIT,ABS), OP(AND,ABS), OP(ROL_M,ABS), OP(RLA,ABS), // $2x
		OP(BMI,REL), OP(AND,IIX), OP(STP  ,IMP), OP(RLA,IIX), OP(NOP,ZPX), OP(AND,ZPX), OP(ROL_M,ZPX), OP(RLA,ZPX), // $3x
		OP(SEC,IMP), OP(AND,ABY), OP(NOP  ,IMP), OP(RLA,ABY), OP(NOP,ABX), OP(AND,ABX), OP(ROL_M,ABX), OP(RLA,ABX), // $3x

		OP(RTI,IMP), OP(EOR,IXI), OP(STP,IMP)  , OP(SRE,IXI), OP(NOP,ZP) , OP(EOR,ZP) , OP(LSR_M,ZP) , OP(SRE,ZP) , // $4x
		OP(PHA,IMP), OP(EOR,IMM), OP(LSR_A,ACC), OP(ALR,IMM), OP(JMP,ABS), OP(EOR,ABS), OP(LSR_M,ABS), OP(SRE,ABS), // $4x
		OP(BVC,REL), OP(EOR,IIX), OP(STP,IMP)  , OP(SRE,IIX), OP(NOP,ZPX), OP(EOR,ZPX), OP(LSR_M,ZPX), OP(SRE,ZPX), // $5x
		OP(CLI,IMP), OP(EOR,ABY), OP(NOP,IMP)  , OP(SRE,ABY), OP(NOP,ABX), OP(EOR,ABX), OP(LSR_M,ABX), OP(SRE,ABX), // $5x
		OP(RTS,IMP), OP(ADC,IXI), OP(STP,IMP)  , OP(RRA,IXI), OP(NOP,ZP) , OP(ADC,ZP) , OP(ROR_M,ZP) , OP(RRA,ZP) , // $6x
		OP(PLA,IMP), OP(ADC,IMM), OP(ROR_A,ACC), OP(ARR,IMM), OP(JMP,IND), OP(ADC,ABS), OP(ROR_M,ABS), OP(RRA,ABS), // $6x
		OP(BVS,REL), OP(ADC,IIX), OP(STP,IMP)  , OP(RRA,IIX), OP(NOP,ZPX), OP(ADC,ZPX), OP(ROR_M,ZPX), OP(RRA,ZPX), // $7x
		OP(SEI,IMP), OP(ADC,ABY), OP(NOP,IMP)  , OP(RRA,ABY), OP(NOP,ABX), OP(ADC,ABX), OP(ROR_M,ABX), OP(RRA,ABX), // $7x

		OP(NOP,IMM), OP(STA,IXI), OP(NOP,IMM), OP(SAX,IXI), OP(STY,ZP) , OP(STA,ZP) , OP(STX,ZP) , OP(SAX,ZP) , // $8x
		OP(DEY,IMP), OP(NOP,IMM), OP(TXA,IMP), OP(XAA,IMM), OP(STY,ABS), OP(STA,ABS), OP(STX,ABS), OP(SAX,ABS), // $8x
		OP(BCC,REL), OP(STA,IIX), OP(STP,IMP), OP(AHX,IIX), OP(STY,ZPX), OP(STA,ZPX), OP(STX,ZPY), OP(SAX,ZPY), // $9x
		OP(TYA,IMP), OP(STA,ABY), OP(TXS,IMP), OP(TAS,ABY), OP(SHY,ABX), OP(STA,ABX), OP(SHX,ABY), OP(AHX,ABY), // $9x
		OP(LDY,IMM), OP(LDA,IXI), OP(LDX,IMM), OP(LAX,IXI), OP(LDY,ZP) , OP(LDA,ZP) , OP(LDX,ZP) , OP(LAX,ZP) , // $Ax
		OP(TAY,IMP), OP(LDA,IMM), OP(TAX,IMP), OP(LAX,IMM), OP(LDY,ABS), OP(LDA,ABS), OP(LDX,ABS), OP(LAX,ABS), // $Ax
		OP(BCS,REL), OP(LDA,IIX), OP(STP,IMP), OP(LAX,IIX), OP(LDY,ZPX), OP(LDA,ZPX), OP(LDX,ZPY), OP(LAX,ZPY), // $Bx
		OP(CLV,IMP), OP(LDA,ABY), OP(TSX,IMP), OP(LAS,ABY), OP(LDY,ABX), OP(LDA,ABX), OP(LDX,ABY), OP(LAX,ABY), // $Bx

		OP(CPY,IMM), OP(CMP,IXI), OP(NOP,IMM), OP(DCP,IXI), OP(CPY,ZP) , OP(CMP,ZP) , OP(DEC,ZP) , OP(DCP,ZP) , // $Cx
		OP(INY,IMP), OP(CMP,IMM), OP(DEX,IMP), OP(AXS,IMM), OP(CPY,ABS), OP(CMP,ABS), OP(DEC,ABS), OP(DCP,ABS), // $Cx
		OP(BNE,REL), OP(CMP,IIX), OP(STP,IMP), OP(DCP,IIX), OP(NOP,ZPX), OP(CMP,ZPX), OP(DEC,ZPX), OP(DCP,ZPX), // $Dx
		OP(CLD,IMP), OP(CMP,ABY), OP(NOP,IMP), OP(DCP,ABY), OP(NOP,ABX), OP(CMP,ABX), OP(DEC,ABX), OP(DCP,ABX), // $Dx
		OP(CPX,IMM), OP(SBC,IXI), OP(NOP,IMM), OP(ISC,IXI), OP(CPX,ZP) , OP(SBC,ZP) , OP(INC,ZP) , OP(ISC,ZP) , // $Ex
		OP(INX,IMP), OP(SBC,IMM), OP(NOP,IMP), OP(SBC,IMM), OP(CPX,ABS), OP(SBC,ABS), OP(INC,ABS), OP(ISC,ABS), // $Ex
		OP(BEQ,REL), OP(SBC,IIX), OP(STP,IMP), OP(ISC,IIX), OP(NOP,ZPX), OP(SBC,ZPX), OP(INC,ZPX), OP(ISC,ZPX), // $Fx
		OP(SED,IMP), OP(SBC,ABY), OP(NOP,IMP), OP(ISC,ABY), OP(NOP,ABX), OP(SBC,ABX), OP(INC,ABX), OP(ISC,ABX)  // $Fx
#undef OP
#undef ABS
#undef ABX
#undef ABY
#undef ACC
#undef IMM
#undef IMP
#undef IXI
#undef IND
#undef IIX
#undef REL
#undef ZP
#undef ZPX
#undef ZPY
	};

	bool bit_to_write_to_irq_disable_flag;
	bool need_irq;
	bool need_nmi; // Whether we need to service an NMI interrupt. Is set right after a negative edge is detected (prev_polled_NMI_line == 1 && polled_NMI_line == 0)
	bool nmi_line; // The NMI signal coming from the ppu.
	bool odd_cpu_cycle; // if the current cpu cycle is odd_numbered
	bool page_crossed;
	bool polled_need_irq;
	bool polled_need_nmi; // Same as above, but this only gets updated to 'need_NMI' only cycle after need_NMI is updated. If this is set after we have executed an instruction, we service the NMI.
	bool polled_nmi_line; // The polled NMI line signal during the 2nd half of the last and second to last CPU cycle, respectively.
	bool prev_polled_nmi_line;
	bool stopped; // set to true by the STP instruction
	bool write_to_irq_disable_flag_before_next_instr;

	/* general-purpose registers */
	u8 A, X, Y;
	/* One bit for each IRQ source (there are eight; https://wiki.nesdev.org/w/index.php?title=IRQ). */
	u8 irq_line;
	u8 opcode;
	u8 operand;
	u8 read_addr;
	/* stack pointer */
	u8 sp;

	u16 addr;
	/* program counter */
	u16 pc;

	struct Status
	{
		bool carry;
		bool zero;
		bool irq_disable;
		bool decimal;
		bool b; /* The B flag; no CPU effect. Actually two bits (this is the lower one). Reads either 10 or 11 in different situations. */
		bool overflow;
		bool neg;
	} status{};

	/* Cycles elapsed during the current call to Update(). */
	uint cpu_cycle_counter;
	/* Writes to certain PPU registers are ignored earlier than ~29658 CPU clocks after reset (on NTSC) */
	uint cpu_cycles_since_reset = 0;
	uint cpu_cycles_until_all_ppu_regs_writable = 29658;
	/* refers to stalling done by the APU when the DMC memory reader reads a byte from PRG */
	uint cpu_cycles_until_no_longer_stalled;
	/* Cycles elapsed since the game was started. */
	uint total_cpu_cycle_counter = 0;

	/// Template definitions ////////////////////////////////////////
	template<Instruction instr, AddrMode addr_mode>
	void ExecuteInstruction()
	{
		using enum AddrMode;

		page_crossed = false;

		if constexpr (addr_mode == Implied || addr_mode == Accumulator) {
			ReadCycle(pc); /* Dummy read */
		}
		else if constexpr (addr_mode == Immediate) {
			read_addr = ReadCycle(pc++);
		}
		else if constexpr (addr_mode == ZeroPage || addr_mode == Relative) {
			addr = ReadCycle(pc++);
		}
		else if constexpr (addr_mode == ZeroPageX || addr_mode == ZeroPageY) {
			u8 index = [&] {
				if constexpr (addr_mode == ZeroPageX) return X;
				else return Y;
			}();
			addr = ReadCycle(pc++);
			ReadCycle(addr); /* Dummy read */
			addr = (addr + index) & 0xFF;
		}
		else if constexpr (addr_mode == Absolute) {
			u8 addr_lo = ReadCycle(pc++);
			u8 addr_hi = ReadCycle(pc++);
			addr = addr_hi << 8 | addr_lo;
		}
		else if constexpr (addr_mode == AbsoluteX || addr_mode == AbsoluteY) {
			u8 index = [&] {
				if constexpr (addr_mode == AbsoluteX) return X;
				else return Y;
			}();
			u8 addr_lo = ReadCycle(pc++);
			u8 addr_hi = ReadCycle(pc++);
			page_crossed = addr_lo + index > 0xFF;
			addr_lo += index;
			addr = addr_hi << 8 | addr_lo;
			read_addr = ReadCycle(addr);
			addr += page_crossed << 8; /* Potentially add 1 to the upper address byte */
		}
		else if constexpr (addr_mode == Indirect) {
			u8 addr_lo = ReadCycle(pc++);
			u8 addr_hi = ReadCycle(pc++);
			addr = addr_hi << 8 | addr_lo;
			u16 addr_tmp = ReadCycle(addr);
			// HW bug: if 'addr' is xyFF, then the upper byte of 'addr_tmp' is fetched from xy00, not (xy+1)00
			addr_tmp |= [&] {
				if (addr_lo == 0xFF) {
					return ReadCycle(addr_hi << 8);
				}
				else {
					return ReadCycle(addr + 1);
				}
			}() << 8;
			addr = addr_tmp;
		}
		else if constexpr (addr_mode == IndexedIndirect) {
			u8 addr_lo = ReadCycle(pc++);
			ReadCycle(addr_lo); /* Dummy read */
			addr_lo += X;
			read_addr = ReadCycle(addr_lo);
			++addr_lo;
			u8 addr_hi = ReadCycle(addr_lo);
			addr = addr_hi << 8 | read_addr;
		}
		else if constexpr (addr_mode == IndirectIndexed) {
			u8 addr_lo = ReadCycle(pc++);
			read_addr = ReadCycle(addr_lo);
			++addr_lo;
			u8 addr_hi = ReadCycle(addr_lo);
			page_crossed = read_addr + Y > 0xFF;
			addr_lo = read_addr + Y;
			addr = addr_hi << 8 | addr_lo;
			read_addr = ReadCycle(addr);
			addr += page_crossed << 8; /* Potentially add 1 to the upper address byte */
		}
		else {
			static_assert(AlwaysFalse<addr_mode>);
		}

		/* Get the operand for read instructions */
		if constexpr (instr == ADC || instr == AND || instr == BIT || instr == CMP || instr == CPX || instr == CPY || instr == EOR ||
			instr == LAS || instr == LAX || instr == LDA || instr == LDX || instr == LDY || instr == NOP || instr == ORA || instr == SBC)
		{
			if constexpr (addr_mode == ZeroPage || addr_mode == ZeroPageX || addr_mode == ZeroPageY || addr_mode == Absolute || addr_mode == IndexedIndirect) {
				operand = ReadCycle(addr);
			}
			else if constexpr (addr_mode == AbsoluteX || addr_mode == AbsoluteY || addr_mode == IndirectIndexed) {
				/* If overflow did not occur when fetching the address, the operand has already been fetched from this address.
					If not, we need to perform the memory read. */
				operand = page_crossed ? ReadCycle(addr) : read_addr;
			}
			else {
				/* For e.g. immediate adressing, the operand has already been read. */
				operand = read_addr;
			}
		}
		/* Get the operand for read-modify-write instructions */
		else if constexpr (addr_mode != Accumulator && (
			instr == ASL_M || instr == DEC || instr == INC || instr == LSR_M || instr == ROL_M || instr == ROR_M ||
			instr == DCP || instr == ISC || instr == RLA || instr == RRA || instr == SLO || instr == SRE))
		{
			/* For these instructions, no matter if a page was crossed when fetching the address, the operand is yet to be read.
				Note: these instructions do not use immediate addressing. */
			operand = ReadCycle(addr);
		}

		instr();
	}
}