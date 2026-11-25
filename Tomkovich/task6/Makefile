CC = gcc
CFLAGS = -O2
TARGET = line_lookup_alarm
SRC = task6.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean

