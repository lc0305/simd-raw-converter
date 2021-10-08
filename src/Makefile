CFLAGS := -Wall -Wextra
CFLAG_BUILD := $(shell cat ../lib/.cflags)
BIN := raw_converter
CFLAG_DEBUG := -Og -g -fsanitize=address -fno-omit-frame-pointer
OBJECT_FILES := main.o convert_file.o queue.o ../lib/convert.o

all: build

debug: CFLAG_BUILD := $(CFLAG_DEBUG)
debug: build

LFLAG_BUILD :=

ifeq ($(OS),Windows_NT)
$(error Windows is NOT supported)
all:
else
UNAME_S := $(shell uname -s | tr '[:upper:]' '[:lower:]')
ifeq ($(UNAME_S),linux)
LFLAG_BUILD := -Wl,--gc-sections
endif
ifeq ($(UNAME_S),darwin)
LFLAG_BUILD := -Wl,-dead_strip
endif
endif

build: $(OBJECT_FILES)
	$(CC) $(CFLAGS) $(CFLAG_BUILD) $(OBJECT_FILES) -lpthread -o $(BIN) $(LFLAG_BUILD)

main.o: main.c
	$(CC) $(CFLAGS) $(CFLAG_BUILD) -c main.c -o main.o

convert_file.o: convert_file.c convert_file.h
	$(CC) $(CFLAGS) $(CFLAG_BUILD) -c convert_file.c -o convert_file.o

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) $(CFLAG_BUILD) -c queue.c -o queue.o

clean:
	rm *.o $(BIN)
