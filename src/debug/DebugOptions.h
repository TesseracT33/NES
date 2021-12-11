#pragma once

//#define DEBUG_LOG

//#define DEBUG_COMPARE_MESEN
//#define DEBUG_COMPARE_MESEN_IRQ
//#define DEBUG_COMPARE_MESEN_NMI
//#define DEBUG_COMPARE_MESEN_PPU

#define DEBUG_LOG_PATH "F:\\nes_trace.log"
#define MESEN_LOG_PATH "C:\\Users\\Christoffer\\Documents\\Emulators\\Mesen.0.9.9\\Debugger\\Trace - Kirby's Adventure (USA) (Rev 1).txt"

#define DEBUG (DEBUG_LOG || DEBUG_COMPARE_MESEN)