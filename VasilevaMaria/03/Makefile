CC = gcc
CFLAGS = -O2 -Wall
TARGET = setuid_test
SRC = setuid_test.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
