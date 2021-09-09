#include "Logging.h"


#ifdef DEBUG_COMPARE_MESEN
Logging::Mesen Logging::mesen{};
#endif

void Logging::Update(CPU* cpu, PPU* ppu)
{
#ifdef DEBUG_LOG
	if (!cpu->curr_instr.instr_executing)
		LogLine(cpu, ppu);
#endif

#ifdef DEBUG_COMPARE_MESEN
	if (!cpu->curr_instr.instr_executing)
		CompareMesenLog(cpu, ppu);
#endif
}


bool Logging::TestString(const std::string& log_line, unsigned line_num, 
	const std::string& sub_str, int emu_value, size_t value_size)
{
	size_t first_char_pos = log_line.find(sub_str + ":") + sub_str.length() + 1;
	std::string reg_str = log_line.substr(first_char_pos, 2 * value_size);
	int log_val = std::stoi(reg_str, nullptr, 16);
	if (log_val != emu_value)
	{
		const char* msg{};
		switch (value_size)
		{
		case 1: msg = "Incorrect %s at line %u; expected $%02X, got $%02X"; break;
		case 2: msg = "Incorrect %s at line %u; expected $%04X, got $%04X"; break;
		}
		wxMessageBox(wxString::Format(msg, sub_str, line_num, log_val, emu_value));
		return false;
	}
	return true;
}


#ifdef DEBUG_LOG
void Logging::LogLine(CPU* cpu, PPU* ppu)
{
	static std::ofstream ofs{ DEBUG_LOG_PATH, std::ofstream::out };
	char buf[100]{};
	sprintf(buf, "#cycle %llu \t PC:%04X \t OP:%02X \t S:%02X  A:%02X  X:%02X  Y:%02X  P:%02X",
		cpu->cpu_cycle_counter, (int)cpu->PC, (int)cpu->GetOpcode(), (int)cpu->S, (int)cpu->A,
		(int)cpu->X, (int)cpu->Y, (int)cpu->GetStatusRegInterrupt());
	ofs << buf << std::endl;
}
#endif


#ifdef DEBUG_COMPARE_MESEN
void Logging::CompareMesenLog(CPU* cpu, PPU* ppu)
{
	// Each line in the mesen trace log should be of the following form:
	// 8000 $78    SEI                A:00 X:00 Y:00 P:04 S:FD CYC:27  SL:0   CPU Cycle:8

	// Get the next line in the mesen trace log
	static std::ifstream ifs{ MESEN_LOG_PATH, std::ifstream::in };
	if (ifs.eof())
	{
		wxMessageBox("Mesen trace log comparison passed. Stopping the cpu.");
		cpu->stopped = true;
		return;
	}
	std::getline(ifs, mesen.current_line);
	mesen.line_counter++;

	// Test PC
	std::string mesen_pc_str = mesen.current_line.substr(0, 4);
	u16 mesen_pc = std::stoi(mesen_pc_str, nullptr, 16);
	if (cpu->PC != mesen_pc)
	{
		wxMessageBox(wxString::Format("Incorrect PC at line %u; expected $%04X, got $%04X", mesen.line_counter, (int)mesen_pc, (int)cpu->PC));
		return;
	}

	// Test cpu cycle
	size_t first_char_pos = mesen.current_line.find("CPU Cycle:") + 10;
	std::string mesen_cycle_str = mesen.current_line.substr(first_char_pos);
	unsigned mesen_cycle = std::stol(mesen_cycle_str);
	if (cpu->cpu_cycle_counter + mesen.cpu_cycle_offset != mesen_cycle)
	{
		wxMessageBox(wxString::Format("Incorrect cycle count at line %u; expected %u, got %u", mesen.line_counter, mesen_cycle, cpu->cpu_cycle_counter + mesen.cpu_cycle_offset));
		return;
	}

	// Test registers
	TestString(mesen.current_line, mesen.line_counter, "A", cpu->A, sizeof u8);
	TestString(mesen.current_line, mesen.line_counter, "X", cpu->X, sizeof u8);
	TestString(mesen.current_line, mesen.line_counter, "Y", cpu->Y, sizeof u8);
	TestString(mesen.current_line, mesen.line_counter, "S", cpu->S, sizeof u8);
	TestString(mesen.current_line, mesen.line_counter, "P", cpu->GetStatusRegInterrupt(), sizeof u8);
}
#endif