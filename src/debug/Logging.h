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
	static void ReportPpuState(unsigned scanline, unsigned ppu_cycle_counter);
	static void Update();

private:
	enum class NumberFormat
	{
		uint8_hex, uint16_hex, uint32_dec, uint64_dec
	};

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
		unsigned scanline;
		unsigned ppu_cycle_counter;
	} ppu_state;

	static bool TestString(const std::string& log_line, unsigned line_num,
		const std::string& sub_str, int emu_value, NumberFormat num_format);

#ifdef DEBUG_LOG
	static std::ofstream log_ofs;
	static void LogLine();
	static void LogNMILine();
#endif

#ifdef DEBUG_COMPARE_MESEN
	static void CompareMesenLogLine();
#endif
};