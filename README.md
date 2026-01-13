# 🏎️ RetroRacer

<div align="center">

**A retro-style 3D racing game for the SEGA Dreamcast**

*Inspired by the golden era of N64 and PS1 racing games*

[![Platform](https://img.shields.io/badge/Platform-Dreamcast-blue)]()
[![License](https://img.shields.io/badge/License-MIT-green)]()
[![Built with](https://img.shields.io/badge/Built%20with-KallistiOS-orange)]()

</div>

---

## 📖 About

RetroRacer is a love letter to the arcade racing games of the late 90s. Built from the ground up for the SEGA Dreamcast using KallistiOS, it features procedurally generated tracks, N64-style low-poly graphics, and fast arcade action.

Every race is unique thanks to the procedural track generation system - no two races are ever the same!

## ✨ Features

- **🎮 4 Game Modes** - Something for everyone
- **🛣️ Procedural Tracks** - Infinite variety with randomly generated circuits
- **🤖 AI Opponents** - Race against up to 7 computer-controlled vehicles
- **🏆 Championship Mode** - Compete in a 4-race Grand Prix series
- **⏱️ Time Trials** - Perfect your racing line and beat your best times
- **🎨 Retro Aesthetics** - Authentic N64-era low-poly graphics

---

## 🎮 Game Modes

### 1. 👁️ AI Race (Spectator Mode)
Sit back and watch AI drivers compete against each other. Perfect for demos or just enjoying the procedural tracks. Choose from 4 difficulty levels to see different racing strategies.

### 2. 🏁 Single Track
Jump right into the action! Race a single procedurally generated track against AI opponents. Quick and satisfying gameplay.

### 3. ⏱️ Time Trial
No opponents, no distractions - just you against the clock. Master the track and shave seconds off your best lap time. Ghost data tracks your personal best.

### 4. 🏆 Grand Prix
The ultimate test! Compete in a 4-race championship series. Earn points based on your finishing position across all races. Different tracks each race keep you on your toes.

**Points System:**
| Position | 1st | 2nd | 3rd | 4th | 5th | 6th | 7th | 8th |
|----------|-----|-----|-----|-----|-----|-----|-----|-----|
| Points   | 10  | 8   | 6   | 5   | 4   | 3   | 2   | 1   |

---

## 🚗 Vehicle Classes

Choose your ride! Each vehicle class offers a different driving experience:

| Class | Top Speed | Acceleration | Handling | Best For |
|-------|-----------|--------------|----------|----------|
| **Standard** | ★★★☆☆ | ★★★☆☆ | ★★★☆☆ | Beginners |
| **Speed** | ★★★★★ | ★★★★☆ | ★★☆☆☆ | Straights |
| **Handling** | ★★☆☆☆ | ★★★☆☆ | ★★★★★ | Tight corners |
| **Balanced** | ★★★★☆ | ★★★★☆ | ★★★★☆ | All-rounders |

---

## 🎯 Controls

### Dreamcast Controller

```
                    ┌─────────────────────────────────────┐
                    │            DREAMCAST                │
    ┌───────────────┼─────────────────────────────────────┼───────────────┐
    │               │                                     │               │
    │   Analog      │           ┌─────────┐               │    X  Y       │
    │   Stick       │           │  START  │               │               │
    │     ○         │           └─────────┘               │    A  B       │
    │               │                                     │               │
    │   D-Pad       │                                     │               │
    │   ┌───┐       │                                     │               │
    │   │ ▲ │       │                                     │               │
    │ ┌─┼───┼─┐     │                                     │               │
    │ │◄│   │►│     │                                     │               │
    │ └─┼───┼─┘     │                                     │               │
    │   │ ▼ │       │                                     │               │
    │   └───┘       │                                     │               │
    └───────────────┴─────────────────────────────────────┴───────────────┘
          │                                                      │
     Left Trigger                                          Right Trigger
       (Brake)                                              (Throttle)
```

### Racing Controls

| Control | Action |
|---------|--------|
| **Analog Stick** / **D-Pad ◄►** | Steering |
| **A Button** / **Right Trigger** | Accelerate |
| **B Button** / **Left Trigger** | Brake |
| **Start** | Pause Game |

### Menu Controls

| Control | Action |
|---------|--------|
| **D-Pad ▲▼** | Navigate menu |
| **A** / **Start** | Select |
| **B** | Back |

### Exit Game
Hold **A + B + X + Y + Start** simultaneously to exit to Dreamcast BIOS.

---

## 🏗️ Building

### Quick Start (Windows)

The easiest way to build on Windows:

```batch
# One-time setup (installs Docker Desktop)
scripts\windows\setup.bat

# Build the game
build.bat
```

### Quick Start (Linux/Mac)

Using Docker (no setup required):
```bash
./scripts/docker-build.sh
```

Or install the full toolchain:
```bash
./scripts/setup-kos.sh
source scripts/env.sh
./scripts/build.sh
```

### Build Targets

| Command | Description |
|---------|-------------|
| `build.bat` / `./scripts/build.sh` | Build ELF executable |
| `build.bat cdi` | Create bootable disc image |
| `build.bat clean` | Clean build artifacts |
| `build.bat shell` | Interactive build environment |

### Requirements

**Option A: Docker (Recommended)**
- Docker Desktop (Windows/Mac) or Docker Engine (Linux)
- No other dependencies needed!

**Option B: Full Toolchain**
- KallistiOS SDK
- SH4 cross-compiler (sh-elf-gcc)
- ARM cross-compiler (arm-eabi-gcc)

---

## 📁 Project Structure

```
retroracer/
├── 📄 Makefile              # KallistiOS build configuration
├── 📄 build.bat             # Windows build script
├── 📁 include/              # Header files
│   ├── ai.h                 # AI racing system
│   ├── game.h               # Game state management
│   ├── input.h              # Controller input
│   ├── math3d.h             # 3D math library
│   ├── menu.h               # Menu system
│   ├── physics.h            # Physics engine
│   ├── render.h             # PVR rendering
│   ├── track.h              # Track generation
│   └── vehicle.h            # Vehicle physics
├── 📁 src/                  # Source files
│   ├── main.c               # Entry point
│   ├── game.c               # Core game logic
│   ├── ai.c                 # AI behavior
│   ├── input.c              # Input handling
│   ├── math3d.c             # Vector/matrix math
│   ├── menu.c               # Menu UI
│   ├── physics.c            # Collision detection
│   ├── render.c             # Graphics rendering
│   ├── track.c              # Procedural track generation
│   └── vehicle.c            # Vehicle dynamics
├── 📁 scripts/
│   ├── setup-kos.sh         # KallistiOS installer
│   ├── build.sh             # Linux/Mac build script
│   ├── docker-build.sh      # Docker build script
│   └── 📁 windows/          # Windows-specific scripts
└── 📁 assets/               # Game assets (models, textures)
```

---

## 🔧 Technical Details

### Engine Features

- **Rendering**: PowerVR hardware-accelerated 3D graphics
- **Resolution**: 640×480 @ 60fps target
- **Physics**: Arcade-style vehicle dynamics with grip simulation
- **AI**: Path-following with overtaking and difficulty scaling
- **Tracks**: Procedural generation with straights, curves, and elevation

### Procedural Track Generation

Tracks are generated using a seeded random algorithm that creates:
- **Straight sections** - Variable length high-speed zones
- **Curves** - Left and right turns up to 45°
- **Hills** - Elevation changes for jumps and dips
- **Checkpoints** - Lap timing and progress tracking

Each difficulty level affects track complexity:
- Easy: Gentle curves, mostly flat
- Medium: Moderate curves, some hills
- Hard: Sharp turns, significant elevation
- Expert: Technical circuits with maximum variety

---

## 📜 License

This project is open source. Feel free to learn from it, modify it, and share it!

---

## 🙏 Credits

- **KallistiOS** - Dreamcast development library
- Inspired by classic racers: Ridge Racer, Daytona USA, Mario Kart 64

---

<div align="center">

**Made with ❤️ for the Dreamcast community**

*Keep the dream alive!*

</div>
