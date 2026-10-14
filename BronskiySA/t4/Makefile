CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = list_strings

all: $(TARGET)

$(TARGET): list_strings.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ list_strings.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
