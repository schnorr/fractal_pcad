#include <stdio.h>
#include "mandelbrot.h"

int mandelbrot(double real, double imag, int max_depth) {
  double complex c = real + imag * I;
  double complex z = 0 + 0 * I;
  int iter = 0;
  while (cabs(z) <= 2.0 && iter < max_depth) {
    z = z * z + c;
    iter++;
  }
  return iter;
}
