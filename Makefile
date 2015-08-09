SHELL = /bin/sh

CC = gcc
CFLAGS = -O2 --std=c99 -pedantic -g
LDFLAGS =
TARGET = wm

.PHONY: all
all: $(TARGET)

$(TARGET): wm.o log.o
	$(CC) $(LDFLAGS) $^ -o $@ -lX11

wm.o: wm.c wm.h log.o
	$(CC) $(CFLAGS) -c $< -o $@

log.o: log.c log.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) *.o $(TARGET)
