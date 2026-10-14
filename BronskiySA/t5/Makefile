CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = line_lookup

all: $(TARGET)

$(TARGET): line_lookup.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ line_lookup.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
