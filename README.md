# CHIP-8 Emulator

A fully-featured CHIP-8 emulator/interpreter written in C using SDL2 for graphics, input, and audio.

## Features

- **Full CHIP-8 instruction set** - All 35 original CHIP-8 opcodes implemented
- **Accurate emulation** - Supports original CHIP-8 quirks and behaviors
- **Audio support** - Square wave sound generation with configurable frequency
- **Smooth graphics** - Color interpolation for pixel fade effects
- **Pixel outlines** - Optional retro CRT-style pixel borders
- **Pause/Resume** - Pause emulation at any time
- **Reset** - Restart the current ROM without reloading
- **Debug mode** - Compile with `-DDEBUG` to see instruction-by-instruction execution

## Requirements

- GCC (C17 compatible)
- SDL2 development libraries

### Installing SDL2

**Ubuntu/Debian:**
```bash
sudo apt install libsdl2-dev
```

**Fedora:**
```bash
sudo dnf install SDL2-devel
```

**Arch Linux:**
```bash
sudo pacman -S sdl2
```

**macOS (Homebrew):**
```bash
brew install sdl2
```

## Building

```bash
# Standard build
make

# Debug build (prints opcode execution details)
make debug
```

## Usage

```bash
./chip8 <rom_file>
```

**Example:**
```bash
./chip8 Tetris.ch8
```

## Controls

The CHIP-8 uses a 16-key hexadecimal keypad. The keys are mapped as follows:

```
CHIP-8 Keypad        Keyboard Mapping
+-+-+-+-+            +-+-+-+-+
|1|2|3|C|            |1|2|3|4|
+-+-+-+-+            +-+-+-+-+
|4|5|6|D|            |Q|W|E|R|
+-+-+-+-+     =>     +-+-+-+-+
|7|8|9|E|            |A|S|D|F|
+-+-+-+-+            +-+-+-+-+
|A|0|B|F|            |Z|X|C|V|
+-+-+-+-+            +-+-+-+-+
```

### Special Keys

| Key | Action |
|-----|--------|
| `Space` | Pause/Resume emulation |
| `=` | Reset/Restart ROM |

## Included ROMs

| ROM | Description |
|-----|-------------|
| `IBM Logo.ch8` | Classic IBM logo test ROM |
| `Tetris.ch8` | Tetris game |
| `Brix.ch8` | Breakout-style brick game |
| `Brick.ch8` | Another brick breaker variant |
| `UFO.ch8` | UFO shooting game |
| `chip8-test-suite.ch8` | Comprehensive opcode test suite |
| `BC_test.ch8` | BCD instruction test |
| `test_opcode.ch8` | Opcode testing ROM |

## Configuration

Default emulator settings (can be modified in source):

| Setting | Default | Description |
|---------|---------|-------------|
| Window Scale | 20x | Display scaling factor |
| Instructions/sec | 500 | CPU clock speed |
| Foreground Color | White (`0xFFFFFFFF`) | Pixel on color |
| Background Color | Black (`0x000000FF`) | Pixel off color |
| Audio Frequency | 440 Hz | Sound timer beep frequency |
| Sample Rate | 44100 Hz | Audio sample rate |
| Pixel Outlines | Enabled | Retro pixel border effect |

## Technical Details

### Memory Layout
- **4KB RAM** (0x000 - 0xFFF)
- **Font data** loaded at 0x000
- **Programs** loaded at 0x200 (entry point)

### Display
- **64x32 pixels** monochrome display
- XOR-based sprite drawing with collision detection

### Timers
- **Delay timer** - Decrements at 60Hz, used for game timing
- **Sound timer** - Decrements at 60Hz, beeps while non-zero

### Stack
- 12-level stack for subroutine calls

### Registers
- 16 general-purpose 8-bit registers (V0-VF)
- 16-bit index register (I)
- 16-bit program counter (PC)

## License

This project is open source. Feel free to use, modify, and distribute.

## References

- [CHIP-8 Wikipedia](https://en.wikipedia.org/wiki/CHIP-8)
- [quesofuego on YouTube](https://youtube.com/@quesofuego?si=ozay1avpTeem84bW)
