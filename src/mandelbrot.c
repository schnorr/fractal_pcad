/*
This file is part of "Fractal @ PCAD".

"Fractal @ PCAD" is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

"Fractal @ PCAD" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with "Fractal @ PCAD". If not, see
<https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include "mandelbrot.h"

static int is_in_julia(long double x, long double y, double cre, double cim, int max_iter) {
    long double complex z = x + y * I;
    long double complex c = cre + cim * I;
    int i;
    for (i = 0; i < max_iter; ++i) {
        z = z * z + c;
        if (cabsl(z) > 2.0)
            return i; // Escaped
    }
    return i; // Remained bounded
}

int mandelbrot(long double real, long double imag, int max_depth) {
  return is_in_julia(real, imag, 0, 0, max_depth);
  /*
  long double complex c = real + imag * I;
  long double complex z = 0 + 0 * I;
  int iter = 0;
  while (cabsl(z) <= 2.0 && iter < max_depth) {
    z = z * z + c;
    iter++;
  }
  return iter;
  */
}
