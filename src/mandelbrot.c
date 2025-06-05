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

int mandelbrot(long double real, long double imag, int max_depth) {
  long double zr = 0.0, zi = 0.0; // real and imaginary parts
  int iter = 0;
  while (zr * zr + zi * zi <= 4.0 && iter < max_depth) {
    long double zr_squared = zr * zr;
    long double zi_squared = zi * zi;
    zi = 2.0 * zr * zi + imag;
    zr = zr_squared - zi_squared + real;
    iter++;
  }
  return iter;
}