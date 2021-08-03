# Summary
Heavily WIP NES emulator written in C++, using wxWidgets and SDL2.

# Compiling and running
Current external dependencies are wxWidgets and SDL2. I only supply Visual Studio solution files. The project settings were as follows:

C/C++ -- Additional Include Directories:
F:\SDKs\wxWidgets-3.1.3\include\msvc; F:\SDKs\wxWidgets-3.1.3\include; F:\SDKs\SDL2-2.0.12\include

Linker -- Additional Library Directories:
F:\SDKs\wxWidgets-3.1.3\lib\vc_x64_lib; F:\SDKs\SDL2-2.0.12\lib\x64;

Linker -- Input -- Additional Dependencies:
SDL2.lib; SDL2main.lib;