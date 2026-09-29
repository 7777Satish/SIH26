CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Isrc
SDL_CFLAGS := $(shell pkg-config --cflags sdl3)
SDL_LIBS := $(shell pkg-config --libs sdl3)
SOURCES := $(wildcard src/*/*.c)
TARGET := virtual_camera
TEST_TARGET := test_cv_tracker

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) $(SOURCES) -o $@ $(SDL_LIBS) -lm

$(TEST_TARGET): tests/test_cv_tracker.c src/cv_tracker/cv_tracker.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@ -lm

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)

.PHONY: clean test
