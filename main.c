#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // Para usar strings

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>
#include <stb_image_write.h>

#include "confusion.h"
#include "diffusion.h"

typedef struct {
  unsigned char r, g, b;
} Pixel;


typedef struct {
  int width, height; 
  int channels;      
  Pixel* pixels;
} Img;


_Static_assert(sizeof(Pixel) == 3, "Pixel precisa ocupar exatamente 3 bytes");

#define CHAVE_PADRAO "senha-do-grupo-123"

void load(const char* name, Img* pic);
void save(const char* name, const Img* pic);
Img nova_img(const Img* modelo);
size_t qtd_pixels(const Img* pic);

typedef enum {
  ETAPA_CONFUSAO,
  ETAPA_DIFUSAO,
  ETAPA_DESFAZ_DIFUSAO,
  ETAPA_DESFAZ_CONFUSAO
} Etapa;


void aplica_etapa(Etapa etapa, Img* in, Img* out, const char* chave) {
  size_t n = qtd_pixels(in);
  unsigned char* pin = (unsigned char*)in->pixels;
  unsigned char* pout = (unsigned char*)out->pixels;
  int erro = 0;

  switch (etapa) {
    case ETAPA_CONFUSAO:  // muda VALORES, posicoes ficam iguais
      memcpy(pout, pin, n * sizeof(Pixel));
      confusion_apply(pout, in->width, in->height, chave);
      break;
    case ETAPA_DESFAZ_CONFUSAO:
      memcpy(pout, pin, n * sizeof(Pixel));
      confusion_invert(pout, in->width, in->height, chave);
      break;
    case ETAPA_DIFUSAO:  // muda POSICOES, valores ficam iguais
      erro = diffusion_apply(pin, pout, in->width, in->height, chave);
      break;
    case ETAPA_DESFAZ_DIFUSAO:
      erro = diffusion_invert(pin, pout, in->width, in->height, chave);
      break;
  }

  if (erro) {
    printf("Erro: memoria insuficiente para a permutacao da difusao\n");
    exit(1);
  }
}

// encode = confusao, depois difusao
void encode(Img* in, Img* out, const char* chave) {
  Img tmp = nova_img(in);
  aplica_etapa(ETAPA_CONFUSAO, in, &tmp, chave);
  aplica_etapa(ETAPA_DIFUSAO, &tmp, out, chave);
  free(tmp.pixels);
}

// decode = ordem inversa do encode (como uma pilha):
// desfaz a difusao primeiro, depois a confusao
void decode(Img* in, Img* out, const char* chave) {
  Img tmp = nova_img(in);
  aplica_etapa(ETAPA_DESFAZ_DIFUSAO, in, &tmp, chave);
  aplica_etapa(ETAPA_DESFAZ_CONFUSAO, &tmp, out, chave);
  free(tmp.pixels);
}

// Compara duas imagens byte a byte. Retorna a qtd de bytes diferentes,
// ou -1 se as dimensoes nao coincidem.
long compara(const Img* a, const Img* b) {
  if (a->width != b->width || a->height != b->height) return -1;
  size_t tam = qtd_pixels(a) * sizeof(Pixel);
  const unsigned char* pa = (const unsigned char*)a->pixels;
  const unsigned char* pb = (const unsigned char*)b->pixels;
  long diffs = 0;
  for (size_t i = 0; i < tam; i++) {
    if (pa[i] != pb[i]) diffs++;
  }
  return diffs;
}