*IN DEVELOPMENT*

# Game Boy (DMG) Emulator written in C

Currently in development, the goal here is to build a portable DMG
emulator in C. Yes, this has been done before, but I wanted to take
a stab at it.

## Requirements

The following requirements are intended to ease portability across MacOS,
Linux, and Windows at the cost of some peformance

- [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/Download/): common audio API. Download with your OS package manager.
- X Window System: the classic POSIX windowing system. Get your OS's standard implementation, for example, XQuartz on macOS. Linux should already ship with it.

## Build

build: `$ make`

run: `$ ./gbe {rom_file}`

## Controls

- arrow keys: arrow keys
- A: "A" key
- B: "B" key
- Select: spacebar
- Start: every other key
