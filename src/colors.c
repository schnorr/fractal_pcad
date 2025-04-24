#include "colors.h"

cor_t get_color (int patamar, int maximo) {
  cor_t cor;
    
  if (patamar >= maximo) {
    // Cor preta para pontos que "escaparam" do fractal
    cor.r = cor.g = cor.b = 0;
    return cor;
  }

  // Normaliza o patamar para o intervalo [0, 1]
  double t = (double) patamar / maximo;

  // Geração de cores suaves com base em seno (efeito arco-íris)
  cor.r = (unsigned short)(sin(6.28318 * t + 0.0) * 127.5 + 127.5); // fase 0
  cor.g = (unsigned short)(sin(6.28318 * t + 2.094) * 127.5 + 127.5); // fase 2π/3
  cor.b = (unsigned short)(sin(6.28318 * t + 4.188) * 127.5 + 127.5); // fase 4π/3

  return cor;
}

cor_t get_color_viridis(int patamar, int maximo) {
  cor_t cor;

  if (patamar >= maximo || maximo <= 0) {
    cor.r = cor.g = cor.b = 0;
    return cor;
  }

  int index = (int)((double)patamar / maximo * (VIRIDIS_SIZE - 1));
  if (index < 0) index = 0;
  if (index >= VIRIDIS_SIZE) index = VIRIDIS_SIZE - 1;

  return viridis_palette[index];
}
