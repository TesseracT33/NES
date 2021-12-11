#pragma once

#include <format>
#include <fstream>
#include <stdexcept>
#include <string>

#include "../Types.h"

#include "../gui/UserMessage.h"

#include "DebugOptions.h"

// This class is used either for making a trace log of the emulator as it goes along (used with DEBUG_LOG),
// or for, at each cpu instruction step, comparing the emulator state to that of the emulator Mesen (used with DEBUG_COMPARE_MESEN).
// Of course, this relies on a Mesen trace log, whose path is given in DebugOptions.h
// Note: it is a class, not a namespace, for it is a friend to other classes such as CPU and PPU

class Logging
{
public:
	struct APUState
	{
		// todo
	} static apu_state;

	struct CPUState
	{
		u8 A, X, Y, P, opcode;
		u16 SP, PC;
		unsigned cpu_cycle_counter;
		bool NMI, IRQ;
	} static cpu_state;

	struct PPUState
	{
		unsigned scanline;
		unsigned ppu_cycle_counter;
	} static ppu_state;

	static void Update();

private:
	enum class NumberFormat
	{
		uint8_hex, uint16_hex, uint32_dec, uint64_dec
	};

	static bool TestString(const std::string& log_line, unsigned line_num,
		const std::string& sub_str, int emu_value, NumberFormat num_format);

#ifdef DEBUG_LOG
	static std::ofstream log_ofs;
	static void LogLine();
#endif

#ifdef DEBUG_COMPARE_MESEN
	static void CompareMesenLogLine();
#endif
};