CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = editor_lock

all: $(TARGET)

$(TARGET): editor_lock.c
	$(CC) $(CFLAGS) editor_lock.c -o $(TARGET)

clean:
	rm -f $(TARGET)
