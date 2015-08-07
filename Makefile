SHELL = /bin/sh

CC = gcc
CFLAGS = -O2 --std=c99 -pedantic -g
LDFLAGS =
DEPS =
TARGET = wm
LIBS = -lX11

.PHONY: all
all: $(TARGET)

$(TARGET): wm.o $(DEPS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

.PHONY: clean
clean:
	$(RM) *.o $(TARGET)
