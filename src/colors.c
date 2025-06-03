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
#include <math.h>

#include "colors.h"

static cor_t linear_interpolation(cor_t c1, cor_t c2, double alpha) {
  if (alpha <= 0.0) return c1;
  if (alpha >= 1.0) return c2;
  cor_t c;
  c.r = (unsigned short)(0.5 + (1.0 - alpha) * c1.r + alpha * c2.r);
  c.g = (unsigned short)(0.5 + (1.0 - alpha) * c1.g + alpha * c2.g);
  c.b = (unsigned short)(0.5 + (1.0 - alpha) * c1.b + alpha * c2.b);
  return c;
}

// Normalizes depth to [0, 1] range linearly
static double norm_linear(int depth, int max_depth){
  return (double)depth / max_depth;
}

// Normalizes depth to [0, 1] range scaled with a power function (depth/k)^n
// 256 and 1/e were chosen as balanced values, adjustment is possible
// First full cycle happens at depth 512, then slows down gradually so that colors
// keep stably looping as depth increases. The effect of this is as the depth increases, 
// the color palette loops, more often initially then tapering off.
// The values at which t resets are all integers n such that 512 * n^e
static double norm_power_cycle(int depth, int max_depth) {
  ++max_depth; // unused
  return fmod(pow(depth / 256.0, (1.0 / 2.71828)), 1.0);
}

static double norm_log_cycle(int depth, int max_depth) {
  ++max_depth; // unused
  return fmod(log(depth / 256.0 + 1), 1.0);
}

// Functions below should be able to accept values outside [0, 1] range, but not tested

// Maps to rainbow colors using sine wave
static cor_t color_sine(double t){
  cor_t cor;
  cor.r = (unsigned short)(sin(6.28318 * t + 0.0) * 127.5 + 127.5); // fase 0
  cor.g = (unsigned short)(sin(6.28318 * t + 2.094) * 127.5 + 127.5); // fase 2π/3
  cor.b = (unsigned short)(sin(6.28318 * t + 4.188) * 127.5 + 127.5); // fase 4π/3
  return cor;
}

// Maps to predefined viridis palette, interpolating
static cor_t color_viridis(double t) {
  t = t - floor(t); 
  t = t * (VIRIDIS_SIZE - 1);
  int index = (int)t;
  double alpha = t - index;

  if (index >= VIRIDIS_SIZE - 1) {
    index = VIRIDIS_SIZE - 2;
    alpha = 1.0;
  }

  return linear_interpolation(viridis_palette[index], viridis_palette[index + 1], alpha);
}


// Maps to grayscale
static cor_t color_linear_grayscale(double t) {
  t = t - floor(t);
  unsigned short gray =(unsigned short)(t * 255.0); // Modulo for values over 1
  return (cor_t) {gray, gray, gray};
}

// Mirrored normalization below. Grayscale and viridis have very dark colors at 0 and 
// bright at 1. This mirrors it so that the brightest value is at 0.5 for smooth 
// looping, and darkest values are at 0 and 1
static double mirror_t(double t){
  t = t - floor(t); 
  return (t <= 0.5) ? t * 2.0 : 2.0 - 2.0 * t;
}

static cor_t color_viridis_mirrored(double t) {
  t = mirror_t(t);
  t = t * (VIRIDIS_SIZE - 1);
  int index = (int)t;
  double alpha = t - index;

  if (index >= VIRIDIS_SIZE - 1) {
    index = VIRIDIS_SIZE - 2;
    alpha = 1.0;
  }

  return linear_interpolation(viridis_palette[index], viridis_palette[index + 1], alpha);
}

static cor_t color_linear_grayscale_mirrored(double t) {
  t = mirror_t(t);
  unsigned short gray = (unsigned short)(t * 255.0);
  return (cor_t) {gray, gray, gray};
}

static cor_t get_color(int depth, int max_depth, normalize_fn normalize_fn, color_fn color_fn) {
  if (depth >= max_depth) return (cor_t){0, 0, 0};
  double t = normalize_fn(depth, max_depth);
  return color_fn(t);
}

// TODO: histogram coloring/other possibilities?
cor_t get_current_pallette_color(int current_color, int depth, int max_depth){
  switch (current_color) {
    case 0:  return get_color(depth, max_depth, norm_linear, color_sine);
    case 1:  return get_color(depth, max_depth, norm_power_cycle, color_sine);
    case 2:  return get_color(depth, max_depth, norm_log_cycle, color_sine);
  
    case 3:  return get_color(depth, max_depth, norm_linear, color_viridis);
    case 4:  return get_color(depth, max_depth, norm_power_cycle, color_viridis_mirrored);
    case 5:  return get_color(depth, max_depth, norm_log_cycle, color_viridis_mirrored);
  
    case 6:  return get_color(depth, max_depth, norm_linear, color_linear_grayscale);
    case 7: return get_color(depth, max_depth, norm_power_cycle, color_linear_grayscale_mirrored);
    case 8:  return get_color(depth, max_depth, norm_log_cycle, color_linear_grayscale_mirrored);

    default: return get_color(depth, max_depth, norm_linear, color_sine);
  }
}

