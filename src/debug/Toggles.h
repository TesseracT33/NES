#pragma once

#define DEBUG_LOG
#define DEBUG_LOG_PATH "F:\\nes_debug_log.log"

#define DEBUG_COMPARE_MESEN
#define MESEN_LOG_PATH "C:\\Users\\Christoffer\\Documents\\Emulators\\Mesen.0.9.9\\Debugger\\Trace - Super Mario Bros. (World).txt"

#define DEBUG (DEBUG_LOG || DEBUG_COMPARE_MESEN)