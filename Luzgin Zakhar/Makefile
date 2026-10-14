# Makefile для Task 1-5. На Solaris предпочтителен gcc.
CC = gcc
CFLAGS = -std=c99 -O2 -Wall -Wextra -pedantic

.PHONY: all clean
all: Task_1/task1 Task_2/task2 Task_3/task3 Task_4/task4 Task_5/task5

Task_1/task1: Task_1/task1.c
	$(CC) $(CFLAGS) -o $@ $<

Task_2/task2: Task_2/task2.c
	$(CC) $(CFLAGS) -o $@ $<

Task_3/task3: Task_3/task3.c
	$(CC) $(CFLAGS) -o $@ $<

Task_4/task4: Task_4/task4.c
	$(CC) $(CFLAGS) -o $@ $<

Task_5/task5: Task_5/task5.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f Task_1/task1 Task_2/task2 Task_3/task3 Task_4/task4 Task_5/task5
