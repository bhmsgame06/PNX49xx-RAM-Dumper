SRC_DIR := src
BUILD_DIR := bin
OBJS := $(SRC_DIR)/main.o
LIBS := 
CC := gcc
CFLAGS := -O2 -g
LD := gcc
LDFLAGS := $(LIBS)
PREFIX := /usr/local

all: ramdump-49xx

ramdump-49xx: $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(LD) -o $(BUILD_DIR)/$@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

install:
	install -Dm755 $(BUILD_DIR)/ramdump-49xx $(PREFIX)/bin

clean:
	rm -f $(OBJS)
