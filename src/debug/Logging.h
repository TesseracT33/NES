#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include <wx/msgdlg.h>

#include "../Types.h"

#include "DebugOptions.h"

// This class is used either for making a trace log of the emulator as it goes along (used with DEBUG_LOG),
// or for, at each cpu instruction step, comparing the emulator state to that of the emulator Mesen (used with DEBUG_COMPARE_MESEN).
// Of course, this relies on a Mesen trace log, whose path is given in DebugOptions.h
class Logging
{
public:
	static void ReportApuState();
	static void ReportCpuState(u8 A, u8 X, u8 Y, u8 P, u8 opcode, u16 SP, u16 PC, unsigned cpu_cycle_counter, bool NMI);
	static void ReportPpuState();
	static void Update();

private:
	static struct APUState
	{
		// todo
	} apu_state;

	static struct CPUState
	{
		bool NMI;
		u8 A, X, Y, P, opcode;
		u16 SP, PC;
		unsigned cpu_cycle_counter;
	} cpu_state;

	static struct PPUState
	{
		// todo
	} ppu_state;

	static bool TestString(const std::string& log_line, unsigned line_num,
		const std::string& sub_str, int emu_value, size_t value_size);

#ifdef DEBUG_LOG
	static void LogLine();
#endif

#ifdef DEBUG_COMPARE_MESEN
	static void CompareMesenLogLine();
	static const unsigned mesen_cpu_cycle_offset = 8;
#endif
};