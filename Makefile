SRC_DIR := src
BUILD_DIR := bin
OBJS := $(SRC_DIR)/main.o
LIBS := 
CC := gcc
CFLAGS := -O2
LD := gcc
LDFLAGS := $(LIBS)

all: ramdump-49xx

ramdump-49xx: $(OBJS)
	mkdir -p $(BUILD_DIR)
	$(LD) -o $(BUILD_DIR)/$@ $^ $(LDFLAGS)

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

install:
	install $(BUILD_DIR)/ramdump-49xx /usr/local/bin

clean:
	rm -f $(OBJS)
