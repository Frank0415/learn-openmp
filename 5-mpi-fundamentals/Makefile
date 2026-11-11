# MPI wrapper: mpicc locally / on Pi; use MPICC=cc with Cray MPICH loaded.
MPICC ?= mpicc
MPIEXEC ?= mpirun
MPIEXEC_FLAGS ?=
RANKS ?= 4
PROGRAMS := pingpong collector overlap allreduce
BINARIES := $(addprefix build/,$(PROGRAMS))

.PHONY: all clean $(PROGRAMS) $(addprefix run-,$(PROGRAMS))
all: $(BINARIES)

# Keep -O0 fixed for every lab compilation.
build/%: %_skeleton.c Makefile | build
	$(MPICC) -std=gnu11 -Wall -Wextra -Wpedantic -O0 $< -lm -o $@

build:
	mkdir -p $@

$(PROGRAMS): %: build/%

run-pingpong: build/pingpong
	$(MPIEXEC) $(MPIEXEC_FLAGS) -n 2 ./build/pingpong

run-collector: build/collector
	$(MPIEXEC) $(MPIEXEC_FLAGS) -n $(RANKS) ./build/collector

run-overlap: build/overlap
	$(MPIEXEC) $(MPIEXEC_FLAGS) -n $(RANKS) ./build/overlap

run-allreduce: build/allreduce
	$(MPIEXEC) $(MPIEXEC_FLAGS) -n $(RANKS) ./build/allreduce

clean:
	$(RM) $(BINARIES)
