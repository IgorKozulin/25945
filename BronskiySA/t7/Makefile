CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = line_lookup_mmap

all: $(TARGET)

$(TARGET): line_lookup_mmap.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ line_lookup_mmap.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
