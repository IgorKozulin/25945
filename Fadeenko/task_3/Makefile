CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = access

all: $(TARGET)

$(TARGET): access.c
	$(CC) $(CFLAGS) access.c -o $(TARGET)

clean:
	rm -f $(TARGET)
