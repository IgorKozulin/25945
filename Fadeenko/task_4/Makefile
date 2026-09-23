CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = list

all: $(TARGET)

$(TARGET): list.c
	$(CC) $(CFLAGS) list.c -o $(TARGET)

clean:
	rm -f $(TARGET)
