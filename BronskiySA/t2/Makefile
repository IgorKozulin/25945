CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
TARGET = ex_time

all: $(TARGET)

$(TARGET): ex_time.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ ex_time.c $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
