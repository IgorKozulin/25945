CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = line_table_mmap

all: $(TARGET)

$(TARGET): line_table_mmap.c
	$(CC) $(CFLAGS) line_table_mmap.c -o $(TARGET)

clean:
	rm -f $(TARGET)
