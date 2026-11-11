# Homebrew GCC on macOS; use `make CC=gcc` on Raspberry Pi/Linux.
ifeq ($(origin CC),default)
CC := gcc-15
endif
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -Wpedantic
OPENMP_FLAGS ?= -fopenmp

PROGRAMS := build/floyd build/triangle

.PHONY: all clean run-floyd run-triangle
all: $(PROGRAMS)

build:
	mkdir -p $@

build/floyd: floyd_warshall_template.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPENMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/triangle: triangle_template.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OPENMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

THREADS ?= 4
FLOYD_GRAPH ?= data/graph5.txt
TRIANGLE_GRAPH ?= data/graph5_tri.txt

run-floyd: build/floyd
	OMP_DYNAMIC=FALSE OMP_NUM_THREADS=$(THREADS) ./build/floyd $(FLOYD_GRAPH)

run-triangle: build/triangle
	OMP_DYNAMIC=FALSE OMP_NUM_THREADS=$(THREADS) ./build/triangle $(TRIANGLE_GRAPH)

clean:
	$(RM) build/floyd build/triangle
