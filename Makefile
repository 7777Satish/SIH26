CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS := $(shell pkg-config --libs sdl3)
SOURCES := $(wildcard src/*.c)
TARGET := virtual_camera

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(SOURCES) -o $@ $(SDL_LIBS) -lm

clean:
	rm -f $(TARGET)

.PHONY: clean
