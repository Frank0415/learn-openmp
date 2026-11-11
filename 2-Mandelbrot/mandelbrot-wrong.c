#include <float.h>
#include <omp.h>
#include <stdio.h>

#define HEIGHT 64
#define WIDTH 64

static int image[HEIGHT * WIDTH];

static int mandelbrot_pixel(int row, int col) {
    (void)row;
    (void)col;
    return 1;
}

int main(void) {
    long long total_iter = 0;
    long long checksum = 0;

#pragma omp parallel
    {
#pragma omp for schedule(runtime) nowait reduction(+ : total_iter)
        for (int row = 0; row < HEIGHT; row++) {
            for (int col = 0; col < WIDTH; col++) {
                int iter = mandelbrot_pixel(row, col);
                image[row * WIDTH + col] = iter;
                total_iter += iter;
            }
        }

#pragma omp for reduction(+ : checksum)
        for (int k = 0; k < HEIGHT * WIDTH; k++) {
            checksum += image[k];
        }
    }

    printf("total_iter=%lld checksum=%lld\n", total_iter, checksum);
    return 0;
}
