#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#define N 100      // Grid size N x N (keep small if you want to print patterns)
#define STEPS 5000 // Number of generations
int grid[N][N];
int newgrid[N][N];
// Initialize grid randomly with 0s and 1s
void initialize_grid() {
    srand(1234); // fixed seed for reproducibility

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            grid[i][j] = rand() % 2;
        }
    }
}
// Print grid to console
void print_grid() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%c", grid[i][j] ? 'O' : '.'); // O = alive, . = dead
        }
        printf("\n");
    }
    printf("\n");
}
// Count live neighbors of cell (x, y)
int count_neighbors(int x, int y) {
    int count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
                continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < N && ny >= 0 && ny < N) {
                count += grid[nx][ny];
            }
        }
    }
    return count;
}
// Update grid to next generation
// TODO: Students parallelize this function with OpenMP
void update() {
#pragma omp parallel
    {
#pragma omp for collapse(2)
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                int alive_n = count_neighbors(i, j);
                if (grid[i][j]) {
                    if (alive_n == 3 || alive_n == 4) {
                        newgrid[i][j] = 1;
                    } else {
                        newgrid[i][j] = 0;
                    }
                } else {
                    if (alive_n == 2) {
                        newgrid[i][j] = 1;
                    } else {
                        newgrid[i][j] = 0;
                    }
                }
            }
        }

#pragma omp for collapse(2)
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                grid[i][j] = newgrid[i][j];
            }
        }
    }
}

int main() {
    initialize_grid();
    printf("Initial Pattern:\n");
    print_grid();
    double start = omp_get_wtime();
    for (int step = 0; step < STEPS; step++) {
        update();
    }
    double end = omp_get_wtime();
    printf("Simulation finished in %f seconds\n", end - start);
    printf("Final Pattern after %d steps:\n", STEPS);
    print_grid();
    return 0;
}