# Summary
nes-dono is a Nintendo Entertainment System (NES) emulator written in C++ with a graphical interface (using wxWidgets-3). It relies on SDL2 for video, audio and input. 
It's an *attempt* at cycle-accurate APU, CPU and PPU emulation. Currently, the CPU implementation is mostly complete and correct, while the APU and PPU implementations could be much improved.

![nes1](https://thumbs2.imgbox.com/d7/ab/XT59O1N4_t.png)
![nes2](https://thumbs2.imgbox.com/ec/21/IztWylQt_t.png)
![nes3](https://thumbs2.imgbox.com/fc/1a/O7q6qiAy_t.png)
![nes4](https://thumbs2.imgbox.com/b3/9d/ozGC6cpU_t.png)
![nes5](https://thumbs2.imgbox.com/a9/cc/5E6PmZjp_t.png)
![nes6](https://thumbs2.imgbox.com/07/52/qInwMtNF_t.png)

# Current features
- GUI
- Supported mappers are (AxROM, CNROM, MMC1, MMC3, NROM, UxROM).
- Audio output
- Save data reading/writing
- Save states

# WIP/future features
- Controller input support (non-functioning due to SDL and wxWidgets not playing nicely together).

# Passed tests
The emulator passes the following tests:
- blargg_apu_2005.07.30, but not tests '09.reset_timing' and '11.len_reload_timing'
- blargg_nes_cpu_test5
- blargg_ppu_tests_2005.09.15b
- branch_timing_tests
- cpu_dummy_reads
- cpu_dummy_writes
- cpu_exec_space/test_cpu_exec_space_ppuio
- cpu_timing_test6
- instr_misc
- mmc3_test_2, but not test '6-MMC3_alt'
- oam_read
- oam_stress
- ppu_open_bus
- read_joy3
- scrolltest
- sprite_hit_tests_2005.10.05
- sprite_overflow_tests, but not tests '3.Timing' and '4.Obscure'

# Compiling and running
C++20 is required for compiling. Current external dependencies are wxWidgets and SDL2. I only supply Visual Studio solution files. The project settings were as follows:

C/C++ -- Additional Include Directories:
F:\SDKs\wxWidgets-3.1.3\include\msvc; F:\SDKs\wxWidgets-3.1.3\include; F:\SDKs\SDL2-2.0.12\include

Linker -- Additional Library Directories:
F:\SDKs\wxWidgets-3.1.3\lib\vc_x64_lib; F:\SDKs\SDL2-2.0.12\lib\x64;

Linker -- Input -- Additional Dependencies:
SDL2.lib; SDL2main.lib;