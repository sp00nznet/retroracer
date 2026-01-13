# RetroRacer - Dreamcast Racing Game
# KallistiOS Makefile

TARGET = retroracer.elf
OBJS = src/main.o src/game.o src/math3d.o src/render.o src/track.o \
       src/vehicle.o src/ai.o src/menu.o src/input.o src/physics.o

# KallistiOS environment
KOS_CFLAGS += -I./include -DDREAMCAST
KOS_CPPFLAGS += -I./include -DDREAMCAST

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

# Create CDI image for burning
cdi: $(TARGET)
	$(KOS_OBJCOPY) -R .stack -O binary $(TARGET) retroracer.bin
	scramble retroracer.bin 1ST_READ.BIN
	mkisofs -C 0,11702 -V RETRORACER -G $(KOS_BASE)/../IP.BIN -l -o retroracer.iso .
	cdi4dc retroracer.iso retroracer.cdi

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

.PHONY: all clean rm-elf cdi run
