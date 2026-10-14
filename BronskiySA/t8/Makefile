CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = locked_editor

all: $(TARGET)

$(TARGET): locked_editor.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ locked_editor.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
