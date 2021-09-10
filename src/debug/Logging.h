#pragma once

#include "../core/CPU.h"
#include "../core/PPU.h"

#include "Toggles.h"

// This class is used either for making a trace log of the emulator as it goes along (used with DEBUG_LOG),
// or for, at each cpu instruction step, comparing the emulator state to that of the emulator Mesen (used with DEBUG_COMPARE_MESEN).
// Of course, this relies on a Mesen trace log, whose path is given in Toggles.h
class Logging
{
public:
	static void Update(CPU* cpu = nullptr, PPU* ppu = nullptr);

private:
	static bool TestString(const std::string& log_line, unsigned line_num,
		const std::string& sub_str, int emu_value, size_t value_size);

#ifdef DEBUG_LOG
	static void LogLine(CPU* cpu, PPU* ppu);
#endif

#ifdef DEBUG_COMPARE_MESEN
	static void CompareMesenLogLine(CPU* cpu, PPU* ppu);
	static const unsigned mesen_cpu_cycle_offset = 8;
#endif
};