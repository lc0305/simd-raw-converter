TEST_BIN := raw_converter_test
CFLAGS := -Wall -Wextra
CFLAG_TEST := -Os
CFLAG_LIB_CONVERT := -fdata-sections -ffunction-sections -Ofast
CFLAG_DEBUG := -Og -g -fsanitize=address -fno-omit-frame-pointer
OBJECT_FILES := timer.o convert.o

ifeq ($(OS),Windows_NT)
$(error Windows is NOT supported)
all:
else
UNAME_S := $(shell uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M := $(shell uname -m | tr '[:upper:]' '[:lower:]')
ifeq ($(UNAME_S),linux)
SSE4_1_SUPPORT := $(filter-out 0,$(shell grep sse4_1 /proc/cpuinfo | wc -l))
AVX2_SUPPORT := $(filter-out 0,$(shell grep avx2 /proc/cpuinfo | wc -l))
endif
ifeq ($(UNAME_S),darwin)
SSE4_1_SUPPORT := $(filter-out 0,$(shell sysctl -a | grep machdep.cpu | grep SSE4.1 | wc -l))
AVX2_SUPPORT := $(filter-out 0,$(shell sysctl -a | grep machdep.cpu | grep AVX2 | wc -l))
endif
endif

ifeq ($(UNAME_M),$(filter $(UNAME_M),aarch64 arm64))
$(info NEON is supported)
ifeq ($(UNAME_S),darwin)
CFLAG_TEST += -mcpu=apple-m1
CFLAG_LIB_CONVERT += -mcpu=apple-m1
CFLAG_DEBUG += -mcpu=apple-m1
else
CFLAG_TEST += -march=armv8-a
CFLAG_LIB_CONVERT += -march=armv8-a
CFLAG_DEBUG += -march=armv8-a
endif
else
CFLAG_TEST += -march=native
CFLAG_LIB_CONVERT += -fno-tree-vectorize -march=native
CFLAG_DEBUG += -march=native
ifneq ($(SSE4_1_SUPPORT),)
$(info SSE4.1 is supported)
CFLAG_TEST += -msse4.1
CFLAG_LIB_CONVERT += -msse4.1
CFLAG_DEBUG += -msse4.1
ifneq ($(AVX2_SUPPORT),)
$(info AVX2 is supported)
CFLAG_TEST += -mavx2
CFLAG_LIB_CONVERT += -mavx2
CFLAG_DEBUG += -mavx2
else
$(warning AVX2 is NOT supported)
endif
else
$(warning SSE4.1 is NOT supported)
endif
endif

all: build_test write_flags

debug: CFLAG_TEST = $(CFLAG_DEBUG)
debug: CFLAG_LIB_CONVERT = $(CFLAG_DEBUG)
debug: build_test

write_flags:
	@echo $(CFLAG_TEST) > .cflags

build_test: $(OBJECT_FILES) test.o
	$(CC) $(CFLAGS) $(CFLAG_TEST) $(OBJECT_FILES) test.o -o $(TEST_BIN)

test.o: test.c
	$(CC) $(CFLAGS) $(CFLAG_TEST) -c test.c -o test.o

timer.o: timer.c timer.h
	$(CC) $(CFLAGS) $(CFLAG_TEST) -c timer.c -o timer.o

convert.o: convert.c convert.h
	$(CC) $(CFLAGS) $(CFLAG_LIB_CONVERT) -c convert.c -o convert.o

clean:
	rm *.o .cflags $(TEST_BIN)
