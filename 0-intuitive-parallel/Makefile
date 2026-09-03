CC         ?= clang
CFLAGS     ?= -O2 -Wall -std=c11

# Homebrew libomp (Apple Silicon). Override with: make OMP_PREFIX=/usr/local/opt/libomp
UNAME_M    := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  OMP_PREFIX ?= /opt/homebrew/opt/libomp
else
  OMP_PREFIX ?= /usr/local/opt/libomp
endif

OMPFLAGS    = -Xpreprocessor -fopenmp -I$(OMP_PREFIX)/include
LDFLAGS     = -L$(OMP_PREFIX)/lib -lomp

NPROC      := $(shell sysctl -n hw.ncpu)

.PHONY: all clean run

all: serial parallel parallel_no_false

serial: serial.c
	$(CC) $(CFLAGS) $(OMPFLAGS) $< $(LDFLAGS) -o $@

parallel: parallel.c
	$(CC) $(CFLAGS) $(OMPFLAGS) $< $(LDFLAGS) -o $@

parallel_no_false: parallel_no_false.c
	$(CC) $(CFLAGS) $(OMPFLAGS) $< $(LDFLAGS) -o $@

run: all
	@echo "=== serial (1 thread) ==="
	./serial
	@echo "=== parallel (false sharing, $(NPROC) threads) ==="
	OMP_NUM_THREADS=$(NPROC) ./parallel
	@echo "=== parallel_no_false (padded, $(NPROC) threads) ==="
	OMP_NUM_THREADS=$(NPROC) ./parallel_no_false

clean:
	rm -f serial parallel parallel_no_false
