/* Lab 2, Tasks 1-3 (SKELETON, fill in the TODOs)
 *
 * Parallel 2D heat-diffusion solver (Jacobi iteration) over a Cartesian
 * process grid. All four physical edges of the GLOBAL domain are held
 * fixed at BOUNDARY_VALUE (a Dirichlet boundary); the interior starts at 0
 * and should converge to BOUNDARY_VALUE everywhere once you're done.
 *
 * Local array layout: each rank owns a (local_rows+2) x (local_cols+2)
 * array, indices 1..local_rows / 1..local_cols are this rank's own data,
 * row/col 0 and local_rows+1/local_cols+1 are the halo ring received from
 * neighbors (or, at a global boundary, permanently fixed to
 * BOUNDARY_VALUE, see the loop already written for you below).
 *
 *   mpicc -o stencil stencil_skeleton.c -lm
 *   mpirun -np 4 ./stencil 32 5000 1e-4
 *
 * argv: global_n (default 32), max_iters (default 5000), epsilon (default 1e-4)
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define BOUNDARY_VALUE 100.0

/*
 * Jacobi 5-point average of one interior cell's 4 neighbors.
 */
static inline double jacobi(double* g, int i, int j, int W) {
    return 0.25 * (g[(i - 1) * W + j] + g[(i + 1) * W + j] +
                   g[i * W + (j - 1)] + g[i * W + (j + 1)]);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int global_n  = (argc > 1) ? atoi(argv[1]) : 32;
    int max_iters = (argc > 2) ? atoi(argv[2]) : 5000;
    double epsilon = (argc > 3) ? atof(argv[3]) : 1e-4;

    /* ---------------------------------------------------------------
     * TODO 1 (Task 1): build a 2D Cartesian topology.
     *
     *   - int dims[2] = {0, 0}; use MPI_Dims_create(size, 2, dims) to let
     *     MPI pick a good PxQ shape for `size` ranks.
     *   - int periods[2] = {0, 0} (no wraparound).
     *   - MPI_Comm cart_comm; build it with MPI_Cart_create (reorder = 1).
     *   - Get this rank's number *within* cart_comm (MPI_Comm_rank on
     *     cart_comm, not MPI_COMM_WORLD) and its coords[2] via
     *     MPI_Cart_coords.
     *   - Find up/down (dimension 0) and left/right (dimension 1) with
     *     two calls to MPI_Cart_shift, displacement 1.
     *
     * Sanity check once you're done: print rank, coords, and
     * up/down/left/right (MPI_PROC_NULL prints as a negative number) and
     * confirm the printed grid shape matches what you'd expect for
     * `size` ranks (e.g. size=4 should give a 2x2 grid with dims[0,0]
     * having up=down=left=... check against a hand-drawn picture).
     * --------------------------------------------------------------- */

    int dims[2] = {0, 0};        /* TODO 1 */
    int coords[2] = {0, 0};      /* TODO 1 */
    int up = MPI_PROC_NULL, down = MPI_PROC_NULL;
    int left = MPI_PROC_NULL, right = MPI_PROC_NULL;   /* TODO 1 */
    MPI_Comm cart_comm = MPI_COMM_WORLD;                /* TODO 1: replace with your real cart_comm */

    if (global_n % dims[0] != 0 || global_n % dims[1] != 0) {
        if (rank == 0)
            fprintf(stderr, "global_n=%d must be divisible by the process grid %dx%d\n",
                    global_n, dims[0], dims[1]);
        MPI_Finalize();
        return 1;
    }

    int local_rows = global_n / dims[0];
    int local_cols = global_n / dims[1];
    int W = local_cols + 2;
    int H = local_rows + 2;
    size_t bytes = (size_t) H * W * sizeof(double);
    double* old_grid = malloc(bytes);
    double* new_grid = malloc(bytes);
    for (int i = 0; i < H * W; i++) { old_grid[i] = 0.0; new_grid[i] = 0.0; }

#define IDX(i, j) ((i) * W + (j))

    /*
     * Fix the halo cells at a GLOBAL physical boundary before the loop starts.
     */
    double* both[2] = {old_grid, new_grid};
    for (int b = 0; b < 2; b++) {
        double* g = both[b];
        if (up    == MPI_PROC_NULL) for (int j = 1; j <= local_cols; j++) g[IDX(0, j)] = BOUNDARY_VALUE;
        if (down  == MPI_PROC_NULL) for (int j = 1; j <= local_cols; j++) g[IDX(local_rows + 1, j)] = BOUNDARY_VALUE;
        if (left  == MPI_PROC_NULL) for (int i = 1; i <= local_rows; i++) g[IDX(i, 0)] = BOUNDARY_VALUE;
        if (right == MPI_PROC_NULL) for (int i = 1; i <= local_rows; i++) g[IDX(i, local_cols + 1)] = BOUNDARY_VALUE;
    }

    /* ---------------------------------------------------------------
     * TODO 2 (Task 2): build a derived datatype describing one strided
     * column of this rank's local array (blocklength 1, count
     * local_rows, stride W), exactly the col_type pattern from lecture.
     * Commit it. Free it near the end of main (after the loop, before
     * MPI_Finalize), it's already there for you.
     * --------------------------------------------------------------- */
    MPI_Datatype col_type;   /* TODO 2: MPI_Type_vector + MPI_Type_commit */

    int iter;
    double global_max_diff = 1e300;
    double t0 = MPI_Wtime();

    for (iter = 1; iter <= max_iters; iter++) {
        MPI_Request reqs[8];

        /* ---------------------------------------------------------------
         *
         * TODO 3 (Task 2): post all 8 non-blocking halo exchange requests
         * into reqs[0..7]: top/bottom rows (contiguous, MPI_DOUBLE,
         * count=local_cols) and left/right columns (col_type, count=1).
         *
         * --------------------------------------------------------------- */

        /* ---------------------------------------------------------------
         * TODO 4 (Task 3): the overlap.
         *
         *   (a) BEFORE MPI_Waitall: update the true interior, cells that
         *       do not touch this rank's own halo ring at all, i.e.
         *       i in [2, local_rows-1], j in [2, local_cols-1]. Call
         *       jacobi(old_grid, i, j, W) and store into new_grid.
         *   (b) MPI_Waitall(8, reqs, MPI_STATUSES_IGNORE);
         *   (c) AFTER MPI_Waitall: update the outer ring of this rank's
         *       owned cells (i==1, i==local_rows, j==1, j==local_cols),
         *       which needed the just-arrived halo data.
         *
         * --------------------------------------------------------------- */

        /* ---------------------------------------------------------------
         * TODO 5 (Task 3): convergence check.
         *
         *   - Compute local_max_diff = the largest |new_grid - old_grid|
         *     over this rank's own cells (i in [1,local_rows], j in
         *     [1,local_cols]).
         *
         * --------------------------------------------------------------- */

        double* tmp = old_grid; old_grid = new_grid; new_grid = tmp;

        if (global_max_diff < epsilon) break;
    }

    double t1 = MPI_Wtime();

    double local_sum = 0.0;
    for (int i = 1; i <= local_rows; i++)
        for (int j = 1; j <= local_cols; j++)
            local_sum += old_grid[IDX(i, j)];
    double local_avg = local_sum / (local_rows * local_cols);

    printf("Rank %d (coords %d,%d): converged=%s after %d iterations, "
           "global_max_diff=%.6f, local avg=%.4f\n",
           rank, coords[0], coords[1], (global_max_diff < epsilon) ? "yes" : "no",
           iter > max_iters ? max_iters : iter, global_max_diff, local_avg);
    if (rank == 0) printf("Total solve time: %.3f ms\n", (t1 - t0) * 1e3);

    MPI_Type_free(&col_type);
    free(old_grid);
    free(new_grid);
    MPI_Finalize();
    return 0;
}
