#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>


int get_offset(int width, int r, int c) {
    return r+(width*c);
}

double get_gauss(int x, int y, double sigma) {
    return (pow(M_E, -1.0*((x*x + y*y)/(2*sigma*sigma))))/sqrt(2*M_PI*sigma*sigma);
}

double* get_kernel(int width, double sigma) {
    double* kernel = malloc(sizeof(double)*width*width);
    int offset = (width-1)/2;
    for (int col = 0; col < width; col++) {
        for (int row = 0; row < width; row++) {
            
            kernel[row*width + col] = get_gauss(row-offset, col-offset, sigma);
        }
    }
    return kernel;
}



int main() {

    

    return 0;
}
