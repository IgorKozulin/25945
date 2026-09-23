CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = line_table

all: $(TARGET)

$(TARGET): line_table.c
	$(CC) $(CFLAGS) line_table.c -o $(TARGET)

clean:
	rm -f $(TARGET)
