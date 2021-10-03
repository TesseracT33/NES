# Summary
nes-dono is a Nintendo Entertainment System (NES) emulator written in C++ with a graphical interface (using wxWidgets-3). It relies on SDL2 for video, audio and input. 
It's an *attempt* at cycle-accurate APU, CPU and PPU emulation. Currently, the CPU implementation is mostly complete and correct, while the APU and PPU implementations could be much improved.

![nes1](https://thumbs2.imgbox.com/d7/ab/XT59O1N4_t.png)
![nes2](https://thumbs2.imgbox.com/ec/21/IztWylQt_t.png)
![nes3](https://thumbs2.imgbox.com/63/94/MwUdqO17_t.png)

# Current features
- GUI
- Supported mappers are (AxROM, CNROM, MMC1, NROM, UxROM).
- Audio output

# WIP/future features
- MMC3 carts.
- Save states.
- Controller input support (non-functioning).

# Compiling and running
Current external dependencies are wxWidgets and SDL2. I only supply Visual Studio solution files. The project settings were as follows:

C/C++ -- Additional Include Directories:
F:\SDKs\wxWidgets-3.1.3\include\msvc; F:\SDKs\wxWidgets-3.1.3\include; F:\SDKs\SDL2-2.0.12\include

Linker -- Additional Library Directories:
F:\SDKs\wxWidgets-3.1.3\lib\vc_x64_lib; F:\SDKs\SDL2-2.0.12\lib\x64;

Linker -- Input -- Additional Dependencies:
SDL2.lib; SDL2main.lib;