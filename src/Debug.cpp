module Debug;

import Bus;

namespace Debug
{
	std::string Disassemble(u16 pc)
	{
		u8 opcode = Bus::Peek(pc);

		switch (opcode) {
		case 0x00: return "rlc  b";
		case 0x01: return "rlc  c";
		case 0x02: return "rlc  d";
		case 0x03: return "rlc  e";
		case 0x04: return "rlc  h";
		case 0x05: return "rlc  l";
		case 0x06: return "rlc  (hl)";
		case 0x07: return "rlc  a";
		case 0x08: return "rrc  b";
		case 0x09: return "rrc  c";
		case 0x0a: return "rrc  d";
		case 0x0b: return "rrc  e";
		case 0x0c: return "rrc  h";
		case 0x0d: return "rrc  l";
		case 0x0e: return "rrc  (hl)";
		case 0x0f: return "rrc  a";
		case 0x10: return "rl   b";
		case 0x11: return "rl   c";
		case 0x12: return "rl   d";
		case 0x13: return "rl   e";
		case 0x14: return "rl   h";
		case 0x15: return "rl   l";
		case 0x16: return "rl   (hl)";
		case 0x17: return "rl   a";
		case 0x18: return "rr   b";
		case 0x19: return "rr   c";
		case 0x1a: return "rr   d";
		case 0x1b: return "rr   e";
		case 0x1c: return "rr   h";
		case 0x1d: return "rr   l";
		case 0x1e: return "rr   (hl)";
		case 0x1f: return "rr   a";
		case 0x20: return "sla  b";
		case 0x21: return "sla  c";
		case 0x22: return "sla  d";
		case 0x23: return "sla  e";
		case 0x24: return "sla  h";
		case 0x25: return "sla  l";
		case 0x26: return "sla  (hl)";
		case 0x27: return "sla  a";
		case 0x28: return "sra  b";
		case 0x29: return "sra  c";
		case 0x2a: return "sra  d";
		case 0x2b: return "sra  e";
		case 0x2c: return "sra  h";
		case 0x2d: return "sra  l";
		case 0x2e: return "sra  (hl)";
		case 0x2f: return "sra  a";
		case 0x30: return "swap b";
		case 0x31: return "swap c";
		case 0x32: return "swap d";
		case 0x33: return "swap e";
		case 0x34: return "swap h";
		case 0x35: return "swap l";
		case 0x36: return "swap (hl)";
		case 0x37: return "swap a";
		case 0x38: return "srl  b";
		case 0x39: return "srl  c";
		case 0x3a: return "srl  d";
		case 0x3b: return "srl  e";
		case 0x3c: return "srl  h";
		case 0x3d: return "srl  l";
		case 0x3e: return "srl  (hl)";
		case 0x3f: return "srl  a";
		case 0x40: return "bit  0,b";
		case 0x41: return "bit  0,c";
		case 0x42: return "bit  0,d";
		case 0x43: return "bit  0,e";
		case 0x44: return "bit  0,h";
		case 0x45: return "bit  0,l";
		case 0x46: return "bit  0,(hl)";
		case 0x47: return "bit  0,a";
		case 0x48: return "bit  1,b";
		case 0x49: return "bit  1,c";
		case 0x4a: return "bit  1,d";
		case 0x4b: return "bit  1,e";
		case 0x4c: return "bit  1,h";
		case 0x4d: return "bit  1,l";
		case 0x4e: return "bit  1,(hl)";
		case 0x4f: return "bit  1,a";
		case 0x50: return "bit  2,b";
		case 0x51: return "bit  2,c";
		case 0x52: return "bit  2,d";
		case 0x53: return "bit  2,e";
		case 0x54: return "bit  2,h";
		case 0x55: return "bit  2,l";
		case 0x56: return "bit  2,(hl)";
		case 0x57: return "bit  2,a";
		case 0x58: return "bit  3,b";
		case 0x59: return "bit  3,c";
		case 0x5a: return "bit  3,d";
		case 0x5b: return "bit  3,e";
		case 0x5c: return "bit  3,h";
		case 0x5d: return "bit  3,l";
		case 0x5e: return "bit  3,(hl)";
		case 0x5f: return "bit  3,a";
		case 0x60: return "bit  4,b";
		case 0x61: return "bit  4,c";
		case 0x62: return "bit  4,d";
		case 0x63: return "bit  4,e";
		case 0x64: return "bit  4,h";
		case 0x65: return "bit  4,l";
		case 0x66: return "bit  4,(hl)";
		case 0x67: return "bit  4,a";
		case 0x68: return "bit  5,b";
		case 0x69: return "bit  5,c";
		case 0x6a: return "bit  5,d";
		case 0x6b: return "bit  5,e";
		case 0x6c: return "bit  5,h";
		case 0x6d: return "bit  5,l";
		case 0x6e: return "bit  5,(hl)";
		case 0x6f: return "bit  5,a";
		case 0x70: return "bit  6,b";
		case 0x71: return "bit  6,c";
		case 0x72: return "bit  6,d";
		case 0x73: return "bit  6,e";
		case 0x74: return "bit  6,h";
		case 0x75: return "bit  6,l";
		case 0x76: return "bit  6,(hl)";
		case 0x77: return "bit  6,a";
		case 0x78: return "bit  7,b";
		case 0x79: return "bit  7,c";
		case 0x7a: return "bit  7,d";
		case 0x7b: return "bit  7,e";
		case 0x7c: return "bit  7,h";
		case 0x7d: return "bit  7,l";
		case 0x7e: return "bit  7,(hl)";
		case 0x7f: return "bit  7,a";
		case 0x80: return "res  0,b";
		case 0x81: return "res  0,c";
		case 0x82: return "res  0,d";
		case 0x83: return "res  0,e";
		case 0x84: return "res  0,h";
		case 0x85: return "res  0,l";
		case 0x86: return "res  0,(hl)";
		case 0x87: return "res  0,a";
		case 0x88: return "res  1,b";
		case 0x89: return "res  1,c";
		case 0x8a: return "res  1,d";
		case 0x8b: return "res  1,e";
		case 0x8c: return "res  1,h";
		case 0x8d: return "res  1,l";
		case 0x8e: return "res  1,(hl)";
		case 0x8f: return "res  1,a";
		case 0x90: return "res  2,b";
		case 0x91: return "res  2,c";
		case 0x92: return "res  2,d";
		case 0x93: return "res  2,e";
		case 0x94: return "res  2,h";
		case 0x95: return "res  2,l";
		case 0x96: return "res  2,(hl)";
		case 0x97: return "res  2,a";
		case 0x98: return "res  3,b";
		case 0x99: return "res  3,c";
		case 0x9a: return "res  3,d";
		case 0x9b: return "res  3,e";
		case 0x9c: return "res  3,h";
		case 0x9d: return "res  3,l";
		case 0x9e: return "res  3,(hl)";
		case 0x9f: return "res  3,a";
		case 0xa0: return "res  4,b";
		case 0xa1: return "res  4,c";
		case 0xa2: return "res  4,d";
		case 0xa3: return "res  4,e";
		case 0xa4: return "res  4,h";
		case 0xa5: return "res  4,l";
		case 0xa6: return "res  4,(hl)";
		case 0xa7: return "res  4,a";
		case 0xa8: return "res  5,b";
		case 0xa9: return "res  5,c";
		case 0xaa: return "res  5,d";
		case 0xab: return "res  5,e";
		case 0xac: return "res  5,h";
		case 0xad: return "res  5,l";
		case 0xae: return "res  5,(hl)";
		case 0xaf: return "res  5,a";
		case 0xb0: return "res  6,b";
		case 0xb1: return "res  6,c";
		case 0xb2: return "res  6,d";
		case 0xb3: return "res  6,e";
		case 0xb4: return "res  6,h";
		case 0xb5: return "res  6,l";
		case 0xb6: return "res  6,(hl)";
		case 0xb7: return "res  6,a";
		case 0xb8: return "res  7,b";
		case 0xb9: return "res  7,c";
		case 0xba: return "res  7,d";
		case 0xbb: return "res  7,e";
		case 0xbc: return "res  7,h";
		case 0xbd: return "res  7,l";
		case 0xbe: return "res  7,(hl)";
		case 0xbf: return "res  7,a";
		case 0xc0: return "set  0,b";
		case 0xc1: return "set  0,c";
		case 0xc2: return "set  0,d";
		case 0xc3: return "set  0,e";
		case 0xc4: return "set  0,h";
		case 0xc5: return "set  0,l";
		case 0xc6: return "set  0,(hl)";
		case 0xc7: return "set  0,a";
		case 0xc8: return "set  1,b";
		case 0xc9: return "set  1,c";
		case 0xca: return "set  1,d";
		case 0xcb: return "set  1,e";
		case 0xcc: return "set  1,h";
		case 0xcd: return "set  1,l";
		case 0xce: return "set  1,(hl)";
		case 0xcf: return "set  1,a";
		case 0xd0: return "set  2,b";
		case 0xd1: return "set  2,c";
		case 0xd2: return "set  2,d";
		case 0xd3: return "set  2,e";
		case 0xd4: return "set  2,h";
		case 0xd5: return "set  2,l";
		case 0xd6: return "set  2,(hl)";
		case 0xd7: return "set  2,a";
		case 0xd8: return "set  3,b";
		case 0xd9: return "set  3,c";
		case 0xda: return "set  3,d";
		case 0xdb: return "set  3,e";
		case 0xdc: return "set  3,h";
		case 0xdd: return "set  3,l";
		case 0xde: return "set  3,(hl)";
		case 0xdf: return "set  3,a";
		case 0xe0: return "set  4,b";
		case 0xe1: return "set  4,c";
		case 0xe2: return "set  4,d";
		case 0xe3: return "set  4,e";
		case 0xe4: return "set  4,h";
		case 0xe5: return "set  4,l";
		case 0xe6: return "set  4,(hl)";
		case 0xe7: return "set  4,a";
		case 0xe8: return "set  5,b";
		case 0xe9: return "set  5,c";
		case 0xea: return "set  5,d";
		case 0xeb: return "set  5,e";
		case 0xec: return "set  5,h";
		case 0xed: return "set  5,l";
		case 0xee: return "set  5,(hl)";
		case 0xef: return "set  5,a";
		case 0xf0: return "set  6,b";
		case 0xf1: return "set  6,c";
		case 0xf2: return "set  6,d";
		case 0xf3: return "set  6,e";
		case 0xf4: return "set  6,h";
		case 0xf5: return "set  6,l";
		case 0xf6: return "set  6,(hl)";
		case 0xf7: return "set  6,a";
		case 0xf8: return "set  7,b";
		case 0xf9: return "set  7,c";
		case 0xfa: return "set  7,d";
		case 0xfb: return "set  7,e";
		case 0xfc: return "set  7,h";
		case 0xfd: return "set  7,l";
		case 0xfe: return "set  7,(hl)";
		case 0xff: return "set  7,a";
		default: std::unreachable();
		}
	}


	std::vector<std::string> Disassemble(u16 pc, size_t num_instructions)
	{
		return { };
	}


	void LogDma(u16 src_addr)
	{
		if (logging_disabled) {
			return;
		}
		log << std::format("DMA started from {:04X}\n", src_addr);
	}


	void LogInstr(u8 opcode, u8 A, u8 X, u8 Y, u8 P, u8 S, u16 PC)
	{
		if (logging_disabled) {
			return;
		}
		auto instr_str = Disassemble(PC);
		log << std::format("{:04X} {} {}  A:{:02X} X:{:02X} Y:{:02X} P:{:02X} S:{:02X}\n",
			PC, opcode, instr_str, A, X, Y, P, S);
	}


	void LogInterrupt(CPU::InterruptType interrupt)
	{
		if (logging_disabled) {
			return;
		}
		log << "Interrupt; " <<
				[&] {
				switch (interrupt) {
				case CPU::InterruptType::BRK: return "BRK";
				case CPU::InterruptType::IRQ: return "IRQ";
				case CPU::InterruptType::NMI: return "NMI";
				default: assert(false); return "";
				}
			}()
			<< '\n';
	}


	void LogIoRead(u16 addr, u8 value)
	{
		if (logging_disabled) {
			return;
		}
		std::string_view reg_name = Bus::IoAddrToString(addr);
		if (reg_name.empty()) {
			log << std::format("IO; {:04X} => {:02X}\n", addr, value);
		}
		else {
			log << std::format("IO; {} => {:02X}\n", reg_name, value);
		}
	}


	void LogIoWrite(u16 addr, u8 value)
	{
		if (logging_disabled) {
			return;
		}
		std::string_view reg_name = Bus::IoAddrToString(addr);
		if (reg_name.empty()) {
			log << std::format("IO; {:04X} <= {:02X}\n", addr, value);
		}
		else {
			log << std::format("IO; {} <= {:02X}\n", reg_name, value);
		}
	}


	void SetLogPath(const std::string& path)
	{
		if (log.is_open()) {
			log.close();
		}
		log.open(path, std::ofstream::out | std::ofstream::binary);
		logging_disabled = !log.is_open();
	}
}