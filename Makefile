UTILS_PATH ?= utils

CC = gcc
CPPFLAGS = -I$(UTILS_PATH)
CFLAGS = -fPIC -Wall -Wextra -g
LDFLAGS = -shared

# Run it with:
# LD_LIBRARY_PATH=./. ./main

SRCS = osmem.c $(UTILS_PATH)/printf.c
OBJS = $(SRCS:.c=.o)
TARGET = libosmem.so

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

pack: clean
	-rm -f src.zip
	-zip -r src.zip *

all:
	$(CC) -I$(UTILS_PATH) $(CFLAGS) -o main main.c -L. -losmem

clean:
	-rm -f src.zip
	-rm -f $(TARGET)
	-rm -f $(OBJS)
	-rm -f main
