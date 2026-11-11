#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INF 1000000000

/*
 * ---------------------------------------------------------
 * Read graph
 * ---------------------------------------------------------
 */

int *read_graph(const char *filename, int *n) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        perror("Cannot open graph file");
        return NULL;
    }

    if (fscanf(fp, "%d", n) != 1) {
        printf("Invalid graph file.\n");
        fclose(fp);
        return NULL;
    }

    size_t size = (size_t)(*n) * (*n);

    int *dist = malloc(size * sizeof(int));

    if (dist == NULL) {
        printf("Memory allocation failed.\n");
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < *n; i++) {
        for (int j = 0; j < *n; j++) {
            if (fscanf(fp, "%d", &dist[i * (*n) + j]) != 1) {
                printf("Invalid graph data.\n");

                free(dist);
                fclose(fp);

                return NULL;
            }
        }
    }

    fclose(fp);

    return dist;
}

/*
 * ---------------------------------------------------------
 * Serial Floyd-Warshall
 * ---------------------------------------------------------
 */

void floyd_serial(int *dist, int n) {
    // Write your serial program for floyd warshall algorithm here
    // dist is an adjacency weight matrix (1D) and n is the number of vertices
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                int pos = i * n + j;
                int pos_ik = i * n + k;
                int pos_kj = k * n + j;
                if (dist[pos_ik] == INF || dist[pos_kj] == INF) {
                    continue;
                }
                dist[pos] = dist[pos] < dist[pos_ik] + dist[pos_kj] ? dist[pos]
                                                                    : dist[pos_ik] + dist[pos_kj];
            }
        }
    }
}

/*
 * ---------------------------------------------------------
 * Optimized parallel Floyd-Warshall
 * ---------------------------------------------------------
 */

int floyd_parallel(int *dist, int n) {
    int *k_row = malloc((size_t)n * sizeof *k_row);
    int *k_col = malloc((size_t)n * sizeof *k_col);
    if (k_row == NULL || k_col == NULL) {
        free(k_row);
        free(k_col);
        fprintf(stderr, "Cannot allocate Floyd-Warshall snapshots.\n");
        exit(EXIT_FAILURE);
    }

    int threads_used = 1;
#pragma omp parallel
    {
#pragma omp single
        threads_used = omp_get_num_threads();

        for (int k = 0; k < n; k++) {
#pragma omp single
            {
                for (int x = 0; x < n; ++x) {
                    k_row[x] = dist[(size_t)k * n + x];
                    k_col[x] = dist[(size_t)x * n + k];
                }
            }

#pragma omp for schedule(static, 1)
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    if (k_col[i] == INF || k_row[j] == INF) {
                        continue;
                    }
                    int candidate = k_col[i] + k_row[j];
                    size_t pos = (size_t)i * n + j;
                    if (candidate < dist[pos]) {
                        dist[pos] = candidate;
                    }
                }
            }
        }
    }
    free(k_row);
    free(k_col);
    return threads_used;
}

/*
 * ---------------------------------------------------------
 * Compare two distance matrices
 * ---------------------------------------------------------
 */

int compare_results(const int *a, const int *b, int n) {
    long long errors = 0;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (a[i * n + j] != b[i * n + j]) {
                if (errors < 10) {
                    printf("Mismatch at [%d][%d]: "
                           "serial=%d parallel=%d\n",
                           i,
                           j,
                           a[i * n + j],
                           b[i * n + j]);
                }

                errors++;
            }
        }
    }

    if (errors > 0) {
        printf("Total mismatches: %lld\n", errors);
        return 0;
    }

    return 1;
}

/*
 * ---------------------------------------------------------
 * Print matrix
 * ---------------------------------------------------------
 */

void print_matrix(const int *dist, int n) {
    if (n > 20) {
        printf("Matrix too large to display.\n");
        return;
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (dist[i * n + j] == INF)
                printf("INF ");
            else
                printf("%3d ", dist[i * n + j]);
        }

        printf("\n");
    }
}

/*
 * ---------------------------------------------------------
 * Main
 * ---------------------------------------------------------
 */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <graph_file>\n", argv[0]);
        printf("Example: %s graph.txt\n", argv[0]);

        return 1;
    }

    const char *filename = argv[1];

    /*
     * -----------------------------------------------------
     * Read original graph
     * -----------------------------------------------------
     */

    int n;

    int *original = read_graph(filename, &n);

    if (original == NULL)
        return 1;

    size_t matrix_size = (size_t)n * n * sizeof(int);

    /*
     * Create two identical copies.
     */
    int *serial = malloc(matrix_size);
    int *parallel = malloc(matrix_size);

    if (serial == NULL || parallel == NULL) {
        printf("Memory allocation failed.\n");

        free(original);
        free(serial);
        free(parallel);

        return 1;
    }

    memcpy(serial, original, matrix_size);
    memcpy(parallel, original, matrix_size);

    free(original);

    /*
     * -----------------------------------------------------
     * Print configuration
     * -----------------------------------------------------
     */

    printf("\n");
    printf("============================================\n");
    printf("       Floyd-Warshall Benchmark\n");
    printf("============================================\n");

    printf("Vertices       : %d\n", n);
    printf("Requested threads: %d\n", omp_get_max_threads());

    /*
     * -----------------------------------------------------
     * Serial
     * -----------------------------------------------------
     */

    double start = omp_get_wtime();

    floyd_serial(serial, n);

    double end = omp_get_wtime();

    double serial_time = end - start;

    printf("\nSerial time    : %.6f seconds\n", serial_time);

    /*
     * -----------------------------------------------------
     * Parallel
     * -----------------------------------------------------
     */

    start = omp_get_wtime();

    int threads_used = floyd_parallel(parallel, n);

    end = omp_get_wtime();

    double parallel_time = end - start;

    printf("Parallel time  : %.6f seconds\n", parallel_time);
    printf("Actual threads : %d\n", threads_used);

    /*
     * -----------------------------------------------------
     * Correctness
     * -----------------------------------------------------
     */

    printf("\nCorrectness    : ");

    int correct = compare_results(serial, parallel, n);
    if (correct)
        printf("PASS\n");
    else
        printf("FAIL\n");

    /*
     * -----------------------------------------------------
     * Performance
     * -----------------------------------------------------
     */

    if (correct && parallel_time > 0.0) {
        double speedup = serial_time / parallel_time;
        double efficiency = 100.0 * speedup / threads_used;

        printf("\nSpeedup        : %.2fx\n", speedup);

        printf("Efficiency     : %.2f%%\n", efficiency);
        printf("Baseline       : serial implementation / OpenMP implementation\n");
    }

    /*
     * -----------------------------------------------------
     * Optional matrix display
     * -----------------------------------------------------
     */

    if (n <= 20) {
        printf("\nShortest-path matrix:\n");

        print_matrix(parallel, n);
    }

    free(serial);
    free(parallel);

    return correct ? EXIT_SUCCESS : EXIT_FAILURE;
}
