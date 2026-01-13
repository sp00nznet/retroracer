# RetroRacer - Dreamcast Racing Game
# KallistiOS Makefile
#
# Build targets:
#   make          - Build for Dreamcast (requires KOS)
#   make native   - Build native version for testing (no KOS required)
#   make clean    - Clean all build artifacts
#   make cdi      - Create bootable CDI disc image
#

# Source files
SRCS = src/main.c src/game.c src/math3d.c src/render.c src/track.c \
       src/vehicle.c src/ai.c src/menu.c src/input.c src/physics.c \
       src/audio.c

# Check if KOS is available
ifdef KOS_BASE
    # ============================================
    # Dreamcast build using KallistiOS
    # ============================================
    TARGET = retroracer.elf
    OBJS = $(SRCS:.c=.o)

    # KallistiOS compiler flags
    KOS_CFLAGS += -I./include -DDREAMCAST
    KOS_CPPFLAGS += -I./include -DDREAMCAST

    all: rm-elf $(TARGET)

    include $(KOS_BASE)/Makefile.rules

    $(TARGET): $(OBJS)
		kos-cc -o $(TARGET) $(OBJS) -lm

    clean: rm-elf
		-rm -f $(OBJS)
		-rm -f retroracer.bin 1ST_READ.BIN retroracer.iso retroracer.cdi

    rm-elf:
		-rm -f $(TARGET)

    # Create CDI image for burning to disc
    cdi: $(TARGET)
		$(KOS_OBJCOPY) -R .stack -O binary $(TARGET) retroracer.bin
		$(KOS_BASE)/utils/scramble/scramble retroracer.bin 1ST_READ.BIN || scramble retroracer.bin 1ST_READ.BIN
		mkisofs -C 0,11702 -V RETRORACER -G $(KOS_BASE)/../IP.BIN -l -o retroracer.iso . || \
		genisoimage -C 0,11702 -V RETRORACER -G $(KOS_BASE)/../IP.BIN -l -o retroracer.iso .
		cdi4dc retroracer.iso retroracer.cdi || echo "cdi4dc not found - ISO created"

    # Run in emulator (if loader configured)
    run: $(TARGET)
		$(KOS_LOADER) $(TARGET)

else
    # ============================================
    # Native build for testing (no Dreamcast)
    # ============================================
    $(info KOS_BASE not set - building native test version)
    $(info For Dreamcast build: source scripts/env.sh && make)
    $(info Or use Docker: ./scripts/docker-build.sh)

    TARGET = retroracer
    OBJS = $(SRCS:.c=.o)

    CC = gcc
    CFLAGS = -Wall -Wextra -O2 -g -I./include
    LDFLAGS = -lm

    all: $(TARGET)

    $(TARGET): $(OBJS)
		$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

    %.o: %.c
		$(CC) $(CFLAGS) -c $< -o $@

    clean:
		-rm -f $(OBJS) $(TARGET)
		-rm -f retroracer.bin 1ST_READ.BIN retroracer.iso retroracer.cdi

    run: $(TARGET)
		./$(TARGET)

endif

# Force native build even if KOS is available
native:
	$(MAKE) -f Makefile.native

.PHONY: all clean rm-elf cdi run native
