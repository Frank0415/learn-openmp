MPICC ?= mpicc
MPIEXEC ?= mpirun
MPIEXEC_FLAGS ?=
RANKS ?= 4
N ?= 32
MAX_ITERS ?= 5000
EPSILON ?= 1e-4

.PHONY: all stencil blocking run run-blocking clean
all: build/stencil
stencil: build/stencil
blocking: build/stencil_blocking

build:
	mkdir -p $@

# Keep both versions at the same optimization level, following Lab 4's convention.
build/stencil: stencil_skeleton.c Makefile | build
	$(MPICC) -std=gnu11 -Wall -Wextra -Wpedantic -O0 $< -lm -o $@

# Create this source from your completed solution when you reach Task 4.
build/stencil_blocking: stencil_blocking.c Makefile | build
	$(MPICC) -std=gnu11 -Wall -Wextra -Wpedantic -O0 $< -lm -o $@

run: build/stencil
	$(MPIEXEC) $(MPIEXEC_FLAGS) -np $(RANKS) ./build/stencil $(N) $(MAX_ITERS) $(EPSILON)

run-blocking: build/stencil_blocking
	$(MPIEXEC) $(MPIEXEC_FLAGS) -np $(RANKS) ./build/stencil_blocking $(N) $(MAX_ITERS) $(EPSILON)

clean:
	$(RM) build/stencil build/stencil_blocking
