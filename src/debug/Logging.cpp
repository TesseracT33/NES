#include "Logging.h"


Logging::APUState Logging::apu_state{};
Logging::CPUState Logging::cpu_state{};
Logging::PPUState Logging::ppu_state{};


void Logging::ReportApuState()
{
	// todo
}


void Logging::ReportCpuState(u8 A, u8 X, u8 Y, u8 P, u8 opcode, u16 SP, u16 PC, unsigned cpu_cycle_counter, bool NMI)
{
	Logging::cpu_state.NMI = NMI;
	Logging::cpu_state.A = A;
	Logging::cpu_state.X = X;
	Logging::cpu_state.Y = Y;
	Logging::cpu_state.P = P;
	Logging::cpu_state.opcode = opcode;
	Logging::cpu_state.SP = SP;
	Logging::cpu_state.PC = PC;
	Logging::cpu_state.cpu_cycle_counter = cpu_cycle_counter;
}


void Logging::ReportPpuState()
{
	// todo
}


void Logging::Update()
{
#ifdef DEBUG_LOG
	LogLine();
#endif

#ifdef DEBUG_COMPARE_MESEN
	CompareMesenLogLine();
#endif
}


// Test the value of 'sub_str' as it is found on a Mesen trace log line 'log_line'.
// 'emu_value' is the corresponding value in our emu (should be either uint8 or uint16), 
// and 'value_size' is the data size of this value (should be either 1 or 2). 
bool Logging::TestString(const std::string& log_line, unsigned line_num, 
	const std::string& substr, int emu_value, size_t value_size)
{
	// Find the corresponding numerical value of 'sub_str', wherever it occurs on the line (e.g. A:FF)
	size_t first_char_pos_of_val = log_line.find(substr + ":") + substr.length() + 1;
	std::string val_str = log_line.substr(first_char_pos_of_val, 2 * value_size);
	int val = std::stoi(val_str, nullptr, 16);

	// Compare the Mesen value against our emu value
	if (val != emu_value)
	{
		const char* msg{};
		switch (value_size)
		{
		case 1: msg = "Incorrect %s at line %u; expected $%02X, got $%02X"; break;
		case 2: msg = "Incorrect %s at line %u; expected $%04X, got $%04X"; break;
		default: throw std::runtime_error("Incorrectly sized argument 'value_size' given to Logging::TestString (expected 1 or 2, got " + value_size); break;
		}
		wxMessageBox(wxString::Format(msg, substr, line_num, val, emu_value));
		return false;
	}
	return true;
}


#ifdef DEBUG_LOG
void Logging::LogLine()
{
	static std::ofstream ofs{ DEBUG_LOG_PATH, std::ofstream::out };
	char buf[100]{};
	sprintf(buf, "#cycle %u \t PC:%04X \t OP:%02X \t SP:%02X  A:%02X  X:%02X  Y:%02X  P:%02X",
		cpu_state.cpu_cycle_counter, (int)cpu_state.PC, (int)cpu_state.opcode, (int)cpu_state.SP, 
		(int)cpu_state.A, (int)cpu_state.X, (int)cpu_state.Y, (int)cpu_state.P);
	ofs << buf << std::endl;
}
#endif


#ifdef DEBUG_COMPARE_MESEN
void Logging::CompareMesenLogLine()
{
	// Each line in the mesen trace log should be something of the following form:
	// 8000 $78    SEI                A:00 X:00 Y:00 P:04 SP:FD CYC:27  SL:0   CPU Cycle:8

	static std::ifstream ifs{ MESEN_LOG_PATH, std::ifstream::in };
	static std::string current_line;
	static unsigned line_counter = 0;

	// Get the next line in the mesen trace log
	if (ifs.eof())
	{
		wxMessageBox("Mesen trace log comparison passed.");
		return;
	}
	std::getline(ifs, current_line);
	line_counter++;

	// Some lines are of a different form: [NMI - Cycle: 206085]
	// Test that an NMI occured here
	if (current_line.find("NMI") != std::string::npos)
	{
		if (!cpu_state.NMI)
			wxMessageBox(wxString::Format("Expected an NMI at line %u.", line_counter));
		return;
	}
	else if (cpu_state.NMI)
		wxMessageBox(wxString::Format("Did not expect an NMI at line %u.", line_counter));

	// Test PC
	std::string mesen_pc_str = current_line.substr(0, 4);
	u16 mesen_pc = std::stoi(mesen_pc_str, nullptr, 16);
	if (cpu_state.PC != mesen_pc)
	{
		wxMessageBox(wxString::Format("Incorrect PC at line %u; expected $%04X, got $%04X", line_counter, (int)mesen_pc, (int)cpu_state.PC));
		return;
	}

	// Test cpu cycle
	size_t first_char_pos = current_line.find("CPU Cycle:") + 10;
	std::string mesen_cycle_str = current_line.substr(first_char_pos);
	unsigned mesen_cycle = std::stol(mesen_cycle_str);
	if (cpu_state.cpu_cycle_counter + mesen_cpu_cycle_offset != mesen_cycle)
	{
		wxMessageBox(wxString::Format("Incorrect cycle count at line %u; expected %u, got %u", line_counter, mesen_cycle, cpu_state.cpu_cycle_counter + mesen_cpu_cycle_offset));
		return;
	}

	// Test registers
	TestString(current_line, line_counter, "A", cpu_state.A, sizeof u8);
	TestString(current_line, line_counter, "X", cpu_state.X, sizeof u8);
	TestString(current_line, line_counter, "Y", cpu_state.Y, sizeof u8);
	TestString(current_line, line_counter, "SP", cpu_state.SP, sizeof u8);
	TestString(current_line, line_counter, "P", cpu_state.P, sizeof u8);
}
#endif