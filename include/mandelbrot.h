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
#ifndef __MANDELBROT_H_
#define __MANDELBROT_H_

#include <complex.h>
#include <math.h>

int mandelbrot(double real, double imag, int max_depth);

#endif
