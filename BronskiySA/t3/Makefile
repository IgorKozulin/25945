CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = setuid_test

all: $(TARGET)

$(TARGET): setuid_test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ setuid_test.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
