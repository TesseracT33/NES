export module Debug;

import CPU;

import NumericalTypes;

import <cassert>;
import <format>;
import <fstream>;
import <string>;
import <string_view>;
import <vector>;

namespace Debug
{
	export
	{
		std::string Disassemble(u16 pc);
		std::vector<std::string> Disassemble(u16 pc, size_t num_instructions);
		void LogDma(u16 src_addr);
		void LogInstr(u8 opcode, u8 A, u8 X, u8 Y, u8 P, u8 S, u16 PC);
		void LogInterrupt(CPU::InterruptType interrupt);
		void LogIoRead(u16 addr, u8 value);
		void LogIoWrite(u16 addr, u8 value);
		void SetLogPath(const std::string& path);

		constexpr bool logging_enabled = true;
		constexpr bool log_instr = logging_enabled && true;
		constexpr bool log_dma = logging_enabled && true;
		constexpr bool log_interrupts = logging_enabled && true;
		constexpr bool log_io = logging_enabled && true;
	}

	bool logging_disabled = true;

	std::ofstream log;
}