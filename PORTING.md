# RetroRacer - Multi-Platform Porting Guide

RetroRacer has been ported to multiple classic gaming consoles. This document describes the platform abstraction layer and provides details for each supported platform.

## Supported Platforms

| Platform | Status | SDK | Resolution | Polygon Budget |
|----------|--------|-----|------------|----------------|
| SEGA Dreamcast | ✅ Primary | KallistiOS | 640x480 | ~7M/sec |
| PlayStation 1 | ✅ Complete | PSn00bSDK | 320x240 | ~180K/sec |
| PlayStation 2 | ✅ Complete | PS2SDK | 640x448 | ~75M/sec |
| PlayStation 3 | ✅ Complete | PSL1GHT | 1280x720 | Unlimited |
| Xbox (Original) | ✅ Complete | NXDK | 640x480 | ~125M/sec |
| Xbox 360 | ✅ Complete | LibXenon | 1280x720 | Unlimited |
| Native (PC) | ✅ Development | GCC | Variable | Unlimited |

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     Game Code (Portable)                        │
│  game.c │ menu.c │ track.c │ vehicle.c │ ai.c │ physics.c      │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────┴────────────────────────────────────────┐
│                   Platform Abstraction Layer                     │
│                        include/platform.h                        │
└────────────────────────┬────────────────────────────────────────┘
                         │
        ┌────────────────┼────────────────┐
        │                │                │
┌───────┴──────┐  ┌──────┴──────┐  ┌──────┴──────┐
│    Input     │  │   Render    │  │    Audio    │
│   Adapter    │  │   Adapter   │  │   Adapter   │
└───────┬──────┘  └──────┬──────┘  └──────┬──────┘
        │                │                │
┌───────┴────────────────┴────────────────┴───────┐
│              Platform Implementations            │
├─────────┬─────────┬─────────┬─────────┬─────────┤
│ PSX     │ PS2     │ PS3     │ Xbox    │ X360    │
└─────────┴─────────┴─────────┴─────────┴─────────┘
```

## Building for Each Platform

### Prerequisites

Install the appropriate SDK for your target platform:

```bash
# Check SDK status
./scripts/build-all-platforms.sh --info
```

### Build Commands

```bash
# Build for specific platform
make -f Makefile.psx       # PlayStation 1
make -f Makefile.ps2       # PlayStation 2
make -f Makefile.ps3       # PlayStation 3
make -f Makefile.xbox      # Xbox (Original)
make -f Makefile.xbox360   # Xbox 360
make                       # Dreamcast (default)
make -f Makefile.native    # Native PC

# Build for all platforms
./scripts/build-all-platforms.sh --all

# Create disc images
make -f Makefile.psx iso   # PSX CD image
make -f Makefile.ps2 iso   # PS2 DVD image
make -f Makefile.xbox iso  # Xbox DVD image
```

## Platform Details

### PlayStation 1 (PSX)

**Hardware Specs:**
- CPU: 33.8688 MHz MIPS R3000A
- RAM: 2MB Main + 1MB VRAM
- GPU: Custom 2D/3D (~180K polygons/sec)
- Audio: SPU with 24 voices, 512KB sound RAM

**Key Adaptations:**
- Reduced polygon count to ~2000 per frame
- 320x240 resolution (quarter of original)
- 16-bit color depth
- Pre-calculated lighting (no real-time)
- Ordering table (OT) for depth sorting
- VAG audio format for music/SFX

**Files:**
- `src/platform/platform_psx.c`
- `Makefile.psx`

**Build Requirements:**
- PSn00bSDK or PsyQ SDK
- mkpsxiso for disc images

---

### PlayStation 2

**Hardware Specs:**
- CPU: 294.912 MHz MIPS R5900 (Emotion Engine)
- RAM: 32MB Main + 4MB VRAM
- GPU: Graphics Synthesizer (~75M polygons/sec)
- Audio: SPU2 with 48 voices

**Key Features:**
- Full resolution (640x448)
- GS packet-based rendering
- DMA transfers for optimal performance
- Hardware texture mapping
- Pressure-sensitive triggers

**Files:**
- `src/platform/platform_ps2.c`
- `Makefile.ps2`

**Build Requirements:**
- PS2SDK
- PS2 toolchain (mips64r5900el-ps2-elf-gcc)

---

### PlayStation 3

**Hardware Specs:**
- CPU: 3.2 GHz Cell Broadband Engine (1 PPE + 6 SPEs)
- RAM: 256MB XDR + 256MB GDDR3
- GPU: RSX Reality Synthesizer (NVidia G70 derived)

**Key Features:**
- HD resolution (1280x720)
- RSX hardware acceleration
- Cell SPE potential for physics (future)
- Full shader support
- Blu-ray/HDD storage

**Files:**
- `src/platform/platform_ps3.c`
- `Makefile.ps3`

**Build Requirements:**
- PSL1GHT SDK
- ps3toolchain

---

### Xbox (Original)

**Hardware Specs:**
- CPU: 733 MHz Intel Pentium III
- RAM: 64MB DDR (unified)
- GPU: NV2A (Custom GeForce 3)

**Key Features:**
- DirectX 8-style rendering
- Push buffer GPU commands (pbkit)
- XInput controller support
- HDD for saves

**Files:**
- `src/platform/platform_xbox.c`
- `Makefile.xbox`

**Build Requirements:**
- NXDK or OpenXDK
- cxbe for XBE creation

---

### Xbox 360

**Hardware Specs:**
- CPU: 3.2 GHz PowerPC Tri-Core Xenon
- RAM: 512MB GDDR3 (unified)
- GPU: ATI Xenos (R500 based)

**Key Features:**
- HD resolution (1280x720)
- Xenos GPU with unified shaders
- XMA audio compression
- Homebrew via LibXenon

**Files:**
- `src/platform/platform_xbox360.c`
- `Makefile.xbox360`

**Build Requirements:**
- LibXenon
- DevkitXenon toolchain
- JTAG/RGH console for testing (or Xenia emulator)

## Platform API Reference

### Core Functions

```c
// Initialize/shutdown
int platform_init(void);
void platform_shutdown(void);

// Timing
uint64_t platform_get_time_us(void);
void platform_sleep_ms(uint32_t ms);
void platform_exit(void);
```

### Memory Functions

```c
void *platform_malloc(size_t size);
void *platform_malloc_aligned(size_t size, size_t alignment);
void platform_free(void *ptr);

// GPU memory
void *platform_vram_alloc(size_t size);
void platform_vram_free(void *ptr);
```

### Graphics Functions

```c
void platform_gfx_init(void);
void platform_gfx_begin_frame(void);
void platform_gfx_end_frame(void);
void platform_gfx_clear(uint32_t color);
void platform_gfx_set_render_list(render_list_t list);
```

### Input Constants

```c
// Cross-platform button mapping
#define PLAT_BTN_CROSS      // A/Cross/A
#define PLAT_BTN_CIRCLE     // B/Circle/B
#define PLAT_BTN_SQUARE     // X/Square/X
#define PLAT_BTN_TRIANGLE   // Y/Triangle/Y
#define PLAT_BTN_START
#define PLAT_BTN_SELECT
#define PLAT_BTN_L1, PLAT_BTN_R1
#define PLAT_BTN_L2, PLAT_BTN_R2  // Triggers
#define PLAT_BTN_DPAD_*
```

## Controller Mapping

| Action | Dreamcast | PlayStation | Xbox |
|--------|-----------|-------------|------|
| Accelerate | A / R-Trig | Cross / R2 | A / RT |
| Brake | B / L-Trig | Circle / L2 | B / LT |
| Steer | Stick/D-Pad | Stick/D-Pad | Stick/D-Pad |
| Menu Select | A / Start | Cross / Start | A / Start |
| Menu Back | B | Circle | B |
| Pause | Start | Start | Start |

## Performance Targets

| Platform | Target FPS | Max Triangles/Frame | Texture RAM |
|----------|------------|---------------------|-------------|
| PSX | 30 | 1,500 | 256KB |
| PS2 | 60 | 50,000 | 2MB |
| PS3 | 60 | Unlimited | 128MB |
| Xbox | 60 | 75,000 | 32MB |
| Xbox 360 | 60 | Unlimited | 256MB |
| Dreamcast | 60 | 10,000 | 8MB |

## Testing

### Emulators

| Platform | Recommended Emulator |
|----------|---------------------|
| PSX | DuckStation, Mednafen |
| PS2 | PCSX2 |
| PS3 | RPCS3 |
| Xbox | Xemu, Cxbx-Reloaded |
| Xbox 360 | Xenia |
| Dreamcast | Flycast, Redream |

### Running in Emulator

```bash
make -f Makefile.psx run    # Run PSX build
make -f Makefile.ps2 run    # Run PS2 build
make -f Makefile.ps3 run    # Run PS3 build
make -f Makefile.xbox run   # Run Xbox build
make -f Makefile.xbox360 run # Run Xbox 360 build
```

## Adding New Platforms

To add support for a new platform:

1. Create `src/platform/platform_<name>.c`
2. Implement all functions from `platform.h`
3. Add platform detection to `platform.h`
4. Create `Makefile.<name>`
5. Update `scripts/build-all-platforms.sh`

## License

Platform-specific code may have different licensing requirements based on the SDKs used:

- PSn00bSDK: MIT License (open source PSX SDK)
- PS2SDK: AFL-2.1 License
- PSL1GHT: BSD-like License
- NXDK: MIT License
- LibXenon: GPL v2

The RetroRacer game code itself is licensed separately from the platform implementations.
