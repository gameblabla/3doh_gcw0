CC = gcc
TARGET = 3doh
CFLAGS = -Ofast -Wall $(SDL_CFLAGS) -I./ -I./freedo -I./freedo/filters  $(shell sdl-config --cflags)
LDFLAGS= -lSDL

OBJS = freedo/arm.o \
freedo/DiagPort.o\
freedo/quarz.o\
freedo/Clio.o \
freedo/frame.o \
freedo/Madam.o \
freedo/vdlp.o \
freedo/_3do_sys.o \
freedo/bitop.o \
freedo/DSP.o \
freedo/Iso.o \
freedo/SPORT.o \
freedo/XBUS.o \
video/sdl/video.o \
sound/sdl/sound.o \
fs/linux/cdrom.o \
timer/linux/timer.o \
input/sdl/input.o \
config.o \
main.o

all: $(TARGET)

rm-elf:
	-rm -f $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -r $(OBJS) $(TARGET)
