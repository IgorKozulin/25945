CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = process_manager

all: $(TARGET)

$(TARGET): process_manager.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ process_manager.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
