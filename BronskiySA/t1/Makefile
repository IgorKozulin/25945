CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = options

all: $(TARGET)

$(TARGET): options.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ options.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
