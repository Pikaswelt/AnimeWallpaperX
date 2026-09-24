CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
TARGET := AnimeWallpaperX

.PHONY: all clean

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

clean:
	rm -f $(TARGET)
