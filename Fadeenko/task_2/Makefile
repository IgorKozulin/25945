CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic
TARGET = ex_time

all: $(TARGET)

$(TARGET): ex_time.c
	$(CC) $(CFLAGS) ex_time.c -o $(TARGET)

clean:
	rm -f $(TARGET)
