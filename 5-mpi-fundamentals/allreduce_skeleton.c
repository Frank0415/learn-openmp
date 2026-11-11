/* Task 4: Ring All-Reduce by Hand (SKELETON, fill in the TODO)
 *
 * Each rank holds a local value. Using ONLY point-to-point communication
 * (no MPI_Reduce/MPI_Allreduce, those are next week's material), compute
 * the GLOBAL SUM of every rank's local value, so that every rank ends up
 * knowing it, not just rank 0.
 *
 * Run with at least 4 ranks:
 *   mpicc -o allreduce allreduce_skeleton.c -lm
 *   mpirun --oversubscribe -np 4 ./allreduce
 */
#include <mpi.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 3) {
        if (rank == 0) fprintf(stderr, "Run with at least 3 ranks.\n");
        MPI_Finalize();
        return 1;
    }

    int right = (rank + 1) % size;
    int left  = (rank - 1 + size) % size;

    double local_value = (double)(rank + 1); /* rank 0 holds 1.0, rank 1 holds 2.0, ... */
    double running_total = local_value;
    double relay = local_value;

    /* TODO: run exactly `size - 1` rounds. In each round:
     *   1. Exchange `relay` with your neighbors using ONE MPI_Sendrecv call:
     *      send `relay` to `right`, receive into a new variable `incoming`
     *      from `left`.
     *   2. Add `incoming` to running_total.
     *   3. Set `relay = incoming`, so next round you forward what you just
     *      received, not your own original value again, continuing the
     *      relay around the ring.
     * After all rounds, running_total should equal the sum of every rank's
     * local_value, on EVERY rank, not just rank 0. */


    for(int i=0; i<size;i++){
        if(i == rank){
            double incoming;
            MPI_Sendrecv(&relay, 1, MPI_DOUBLE, right, 0,
                &incoming, 1, MPI_DOUBLE, left, 0,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            running_total += incoming;
            relay = incoming;
        }
    }
    double expected = size * (size + 1) / 2.0;
    int ok = fabs(running_total - expected) < 1e-9;
    printf("Rank %d: local=%.1f, running_total=%.1f, expected=%.1f -> %s\n",
           rank, local_value, running_total, expected, ok ? "PASS" : "FAIL");

    MPI_Finalize();
    return 0;
}
