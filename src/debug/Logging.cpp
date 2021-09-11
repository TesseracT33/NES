#include "Logging.h"


#ifdef DEBUG_LOG
std::ofstream Logging::log_ofs{ DEBUG_LOG_PATH, std::ofstream::out };
#endif


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


void Logging::ReportPpuState(unsigned scanline, unsigned ppu_cycle_counter)
{
	Logging::ppu_state.scanline = scanline;
	Logging::ppu_state.ppu_cycle_counter = ppu_cycle_counter;
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
// 'emu_value' is the corresponding value in our emu (should be either uint8 or uint16)
bool Logging::TestString(const std::string& log_line, unsigned line_num, 
	const std::string& substr, int emu_value, NumberFormat num_format)
{
	// Find the corresponding numerical value of 'sub_str', wherever it occurs on the line (e.g. A:FF)
	size_t start_pos_of_val = log_line.find(substr + ":") + substr.length() + 1;
	size_t end_pos_of_val = log_line.find(" ", start_pos_of_val);
	std::string val_str;
	if (end_pos_of_val == std::string::npos)
		val_str = log_line.substr(start_pos_of_val);
	else
		val_str = log_line.substr(start_pos_of_val, end_pos_of_val - start_pos_of_val);

	int base;
	switch (num_format)
	{
	case NumberFormat::uint8_hex: case NumberFormat::uint16_hex: base = 16; break;
	case NumberFormat::uint32_dec: case NumberFormat::uint64_dec: base = 10; break;
	}
	int val = std::stoi(val_str, nullptr, base);

	// Compare the Mesen value against our emu value
	if (val != emu_value)
	{
		const char* msg{};
		switch (num_format)
		{
		case NumberFormat::uint8_hex : msg = "Incorrect %s at line %u; expected $%02X, got $%02X"; break;
		case NumberFormat::uint16_hex: msg = "Incorrect %s at line %u; expected $%04X, got $%04X"; break;
		case NumberFormat::uint32_dec: msg = "Incorrect %s at line %u; expected %u, got %u"; break;
		case NumberFormat::uint64_dec: msg = "Incorrect %s at line %u; expected %llu, got %llu"; break;
		}
		wxMessageBox(wxString::Format(msg, substr, line_num, val, emu_value));
		return false;
	}
	return true;
}


#ifdef DEBUG_LOG
void Logging::LogLine()
{
	if (cpu_state.NMI)
	{
		log_ofs << "<<< NMI handled >>>" << std::endl;
		return;
	}

	char buf[100]{};
	sprintf(buf, "CPU cycle %u \t PC:%04X \t OP:%02X \t SP:%02X  A:%02X  X:%02X  Y:%02X  P:%02X  SL:%u  PPU cycle %u",
		cpu_state.cpu_cycle_counter, (int)cpu_state.PC, (int)cpu_state.opcode, (int)cpu_state.SP, 
		(int)cpu_state.A, (int)cpu_state.X, (int)cpu_state.Y, (int)cpu_state.P, ppu_state.scanline, ppu_state.ppu_cycle_counter);
	log_ofs << buf << std::endl;
}


void Logging::LogNMILine()
{
	log_ofs << "NMI handled" << std::endl;
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
	// Check whether an NMI occured here
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
	TestString(current_line, line_counter, "CPU Cycle", cpu_state.cpu_cycle_counter, NumberFormat::uint32_dec);

	// Test cpu registers
	TestString(current_line, line_counter, "A", cpu_state.A, NumberFormat::uint8_hex);
	TestString(current_line, line_counter, "X", cpu_state.X, NumberFormat::uint8_hex);
	TestString(current_line, line_counter, "Y", cpu_state.Y, NumberFormat::uint8_hex);
	TestString(current_line, line_counter, "SP", cpu_state.SP, NumberFormat::uint8_hex);
	TestString(current_line, line_counter, "P", cpu_state.P, NumberFormat::uint8_hex);

	// Test ppu cycle counter and scanline
#ifdef DEBUG_COMPARE_MESEN_PPU
	TestString(current_line, line_counter, "CYC", ppu_state.ppu_cycle_counter, NumberFormat::uint32_dec);
	TestString(current_line, line_counter, "SL", ppu_state.scanline, NumberFormat::uint32_dec);
#endif
}
#endif