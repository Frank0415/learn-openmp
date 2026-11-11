#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ---------------------------------------------------------
 * Read graph
 * ---------------------------------------------------------
 */

unsigned char *read_graph(const char *filename, int *n) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        perror("Cannot open graph file");
        return NULL;
    }

    if (fscanf(fp, "%d", n) != 1) {
        fprintf(stderr, "Invalid graph file.\n");
        fclose(fp);
        return NULL;
    }

    if (*n <= 0) {
        fprintf(stderr, "Invalid number of vertices.\n");
        fclose(fp);
        return NULL;
    }

    size_t size = (size_t)(*n) * (*n);

    unsigned char *adj = malloc(size * sizeof(unsigned char));

    if (adj == NULL) {
        perror("Memory allocation failed");
        fclose(fp);
        return NULL;
    }

    for (int i = 0; i < *n; i++) {
        for (int j = 0; j < *n; j++) {
            int value;

            if (fscanf(fp, "%d", &value) != 1) {
                fprintf(stderr, "Invalid graph data at [%d][%d].\n", i, j);

                free(adj);
                fclose(fp);

                return NULL;
            }

            adj[(size_t)i * (*n) + j] = (unsigned char)value;
        }
    }

    fclose(fp);

    return adj;
}

/*
 * ---------------------------------------------------------
 * Count edges
 * ---------------------------------------------------------
 */

long long count_edges(const unsigned char *adj, int n) {
    long long edges = 0;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (adj[(size_t)i * n + j]) {
                edges++;
            }
        }
    }

    return edges;
}

/*
 * ---------------------------------------------------------
 * Serial triangle counting
 * ---------------------------------------------------------
 */

long long triangle_count_serial(const unsigned char *adj, int n) {
    long long count = 0;
    // We make the order a < b < c avoid double counting
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            if (!adj[a * n + b]) {
                continue;
            }
            for (int c = b + 1; c < n; c++) {
                if (!adj[b * n + c] || !adj[c * n + a]) {
                    continue;
                }
                count++;
            }
        }
    }
    return count;
}

/*
 * ---------------------------------------------------------
 * Parallel triangle counting
 * ---------------------------------------------------------
 */

long long triangle_count_parallel(const unsigned char *adj, int n) {
    // Write your parallel program for triangle count here
    long long count = 0;
#pragma omp parallel for reduction(+ : count) schedule(static, 1)
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            if (!adj[a * n + b]) {
                continue;
            }
            for (int c = b + 1; c < n; c++) {
                if (!adj[b * n + c] || !adj[c * n + a]) {
                    continue;
                }
                count++;
            }
        }
    }
    return count;
}

/*
 * ---------------------------------------------------------
 * Verify that graph is undirected
 * ---------------------------------------------------------
 */

int verify_graph(const unsigned char *adj, int n) {
    for (int i = 0; i < n; i++) {
        if (adj[(size_t)i * n + i] != 0) {
            printf("Error: self-loop at %d\n", i);
            return 0;
        }

        for (int j = i + 1; j < n; j++) {
            if (adj[(size_t)i * n + j] != adj[(size_t)j * n + i]) {
                printf("Error: graph is not symmetric "
                       "at [%d][%d]\n",
                       i,
                       j);

                return 0;
            }
        }
    }

    return 1;
}

/*
 * ---------------------------------------------------------
 * Main
 * ---------------------------------------------------------
 */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <graph_file>\n", argv[0]);
        printf("Example: %s graph1000.txt\n", argv[0]);

        return 1;
    }

    const char *filename = argv[1];

    /*
     * -----------------------------------------------------
     * Read graph
     * -----------------------------------------------------
     */

    int n;

    unsigned char *adj = read_graph(filename, &n);

    if (adj == NULL)
        return 1;

    /*
     * -----------------------------------------------------
     * Validate graph
     * -----------------------------------------------------
     */

    if (!verify_graph(adj, n)) {
        free(adj);

        return 1;
    }

    /*
     * -----------------------------------------------------
     * Graph information
     * -----------------------------------------------------
     */

    long long edges = count_edges(adj, n);

    long long possible_edges = ((long long)n * (n - 1)) / 2;

    double density = (double)edges / possible_edges;

    /*
     * -----------------------------------------------------
     * Configuration
     * -----------------------------------------------------
     */

    printf("\n");
    printf("============================================\n");
    printf("       Triangle Counting Benchmark\n");
    printf("============================================\n");

    printf("Vertices       : %d\n", n);
    printf("Edges          : %lld\n", edges);
    printf("Density        : %.6f\n", density);
    printf("OpenMP threads : %d\n", omp_get_max_threads());

    /*
     * -----------------------------------------------------
     * Serial
     * -----------------------------------------------------
     */

    double start = omp_get_wtime();

    long long serial_count = triangle_count_serial(adj, n);

    double end = omp_get_wtime();

    double serial_time = end - start;

    /*
     * -----------------------------------------------------
     * Parallel
     * -----------------------------------------------------
     */

    start = omp_get_wtime();

    long long parallel_count = triangle_count_parallel(adj, n);

    end = omp_get_wtime();

    double parallel_time = end - start;

    /*
     * -----------------------------------------------------
     * Results
     * -----------------------------------------------------
     */

    printf("\n");
    printf("--------------------------------------------\n");
    printf("Results\n");
    printf("--------------------------------------------\n");

    printf("Triangles      : %lld\n", serial_count);

    printf("Serial time    : %.6f s\n", serial_time);

    printf("Parallel time  : %.6f s\n", parallel_time);

    /*
     * -----------------------------------------------------
     * Correctness
     * -----------------------------------------------------
     */

    printf("\nCorrectness    : ");

    if (serial_count == parallel_count) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");

        printf("Serial result  : %lld\n", serial_count);

        printf("Parallel result: %lld\n", parallel_count);
    }

    /*
     * -----------------------------------------------------
     * Speedup
     * -----------------------------------------------------
     */

    if (parallel_time > 0.0) {
        double speedup = serial_time / parallel_time;

        int threads = omp_get_max_threads();

        double efficiency = speedup / threads;

        printf("\n");
        printf("--------------------------------------------\n");
        printf("Performance\n");
        printf("--------------------------------------------\n");

        printf("Speedup        : %.3fx\n", speedup);

        printf("Efficiency     : %.2f%%\n", efficiency * 100.0);
    }

    free(adj);

    return 0;
}
