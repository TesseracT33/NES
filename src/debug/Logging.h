#pragma once

#include "../core/CPU.h"
#include "../core/PPU.h"

#include "Toggles.h"

// Could have been a namespace since all members are static, but then I can't make it a friend of e.g. CPU
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
	static void CompareMesenLog(CPU* cpu, PPU* ppu);

	static struct Mesen
	{
		std::string current_line;
		unsigned line_counter = 0;
		const unsigned cpu_cycle_offset = 8;
	} mesen;
#endif
};