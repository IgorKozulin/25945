CC = gcc
CFLAGS = -std=c99 -Wall -Wextra
TARGET = start
SRC = catalog.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean