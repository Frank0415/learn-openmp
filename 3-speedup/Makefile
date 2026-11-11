CC         ?= clang
CFLAGS     ?= -O2 -Wall -std=c11

# Homework uses N=1000000000 (~16 GiB for x and y). Default is laptop-sized.
# Override with: make N=1000000000
N          ?= 100000000

# Homebrew libomp (Apple Silicon). Override with: make OMP_PREFIX=/usr/local/opt/libomp
UNAME_M    := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  OMP_PREFIX ?= /opt/homebrew/opt/libomp
else
  OMP_PREFIX ?= /usr/local/opt/libomp
endif

OMPFLAGS    = -Xpreprocessor -fopenmp -I$(OMP_PREFIX)/include
LDFLAGS     = -L$(OMP_PREFIX)/lib -lomp

.PHONY: all clean run

all: partial reduction

partial: partial.c
	$(CC) $(CFLAGS) $(OMPFLAGS) -DN=$(N)L $< $(LDFLAGS) -o $@

reduction: reduction.c
	$(CC) $(CFLAGS) $(OMPFLAGS) -DN=$(N)L $< $(LDFLAGS) -o $@

run: all
	@echo "=== 1 thread baseline (adjacent partial) ==="
	OMP_NUM_THREADS=1 ./partial
	@echo "=== adjacent partial, 8 threads ==="
	OMP_NUM_THREADS=8 ./partial
	@echo "=== reduction, 8 threads ==="
	OMP_NUM_THREADS=8 ./reduction
	@echo "=== reduction, 16 threads ==="
	OMP_NUM_THREADS=16 ./reduction
	@echo "=== reduction, 32 threads ==="
	OMP_NUM_THREADS=32 ./reduction

clean:
	rm -f partial reduction
