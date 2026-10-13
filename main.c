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

int verify(const char* nome_a, const char* nome_b) {
  Img a, b;
  load(nome_a, &a);
  load(nome_b, &b);
  long diffs = compara(&a, &b);
  size_t tam = qtd_pixels(&a) * sizeof(Pixel);
  
  if (diffs < 0) {
    printf("DIFERENTES: dimensoes nao coincidem (%d x %d vs %d x %d)\n",
           a.width, a.height, b.width, b.height);
  } else if (diffs == 0) {
    printf("IDENTICAS: os %zu bytes de pixel de '%s' e '%s' sao byte a byte iguais\n",
           tam, nome_a, nome_b);
  } else {
    printf("DIFERENTES: %ld de %zu bytes de pixel divergem entre '%s' e '%s'\n",
           diffs, tam, nome_a, nome_b);
  }
  
  free(a.pixels);
  free(b.pixels);
  return diffs == 0 ? 0 : 2;
}

// Monta "<nome da imagem sem pasta e sem extensao><sufixo>"
void nome_saida(char* dest, size_t cap, const char* origem, const char* sufixo) {
  const char* base = origem;
  for (const char* p = origem; *p; p++) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  size_t len = strlen(base);
  const char* ponto = strrchr(base, '.');
  if (ponto && ponto != base) len = (size_t)(ponto - base);
  snprintf(dest, cap, "%.*s%s", (int)len, base, sufixo);
}

// Demonstracao completa (modo do codigo-base: ./bagunceitor imagem [chave])
int demo(const char* origem, const char* chave) {
  Img in, out;
  load(origem, &in);
  printf("Origem   : %s %d x %d\n", origem, in.width, in.height);
  printf("Chave    : \"%s\"\n", chave);
  printf("Processando...\n");
  
  char nome[1024];
  out = nova_img(&in);
  
  // 1) So confusao: valores mudam, posicoes nao
  aplica_etapa(ETAPA_CONFUSAO, &in, &out, chave);
  nome_saida(nome, sizeof nome, origem, "_1_so_confusao.png");
  save(nome, &out);
  
  // 2) So difusao (sobre a ORIGINAL): posicoes mudam, cores nao
  aplica_etapa(ETAPA_DIFUSAO, &in, &out, chave);
  nome_saida(nome, sizeof nome, origem, "_2_so_difusao.png");
  save(nome, &out);
  
  // 3) Confusao + difusao = imagem embaralhada
  encode(&in, &out, chave);
  nome_saida(nome, sizeof nome, origem, "_3_embaralhada.png");
  save(nome, &out);
  printf("Bytes diferentes do original: %ld de %zu\n", compara(&in, &out),
         qtd_pixels(&in) * sizeof(Pixel));
  
  // 4) Recupera a partir da embaralhada
  Img rec = nova_img(&in);
  decode(&out, &rec, chave);
  nome_saida(nome, sizeof nome, origem, "_4_recuperada.png");
  save(nome, &rec);
  
  long diffs = compara(&in, &rec);
  if (diffs == 0) {
    printf("IDENTICAS: a imagem recuperada e byte a byte igual a original\n");
  } else {
    printf("DIFERENTES: %ld bytes divergem na recuperacao\n", diffs);
  }
  
  free(in.pixels);
  free(out.pixels);
  free(rec.pixels);
  return diffs == 0 ? 0 : 2;
}

// Um comando do tipo: ./bagunceitor <comando> <entrada> <saida> <chave>
int comando(const char* cmd, const char* origem, const char* destino, const char* chave) {
  Img in, out;
  load(origem, &in);
  printf("Origem   : %s %d x %d\n", origem, in.width, in.height);
  out = nova_img(&in);
  
  if (strcmp(cmd, "encode") == 0) encode(&in, &out, chave);
  else if (strcmp(cmd, "decode") == 0) decode(&in, &out, chave);
  else if (strcmp(cmd, "confusao") == 0) aplica_etapa(ETAPA_CONFUSAO, &in, &out, chave);
  else if (strcmp(cmd, "difusao") == 0) aplica_etapa(ETAPA_DIFUSAO, &in, &out, chave);
  else if (strcmp(cmd, "desfaz-difusao") == 0) aplica_etapa(ETAPA_DESFAZ_DIFUSAO, &in, &out, chave);
  else aplica_etapa(ETAPA_DESFAZ_CONFUSAO, &in, &out, chave);
  
  save(destino, &out);
  free(in.pixels);
  free(out.pixels);
  return 0;
}

void uso(void) {
  printf("Uso:\n"
         "  bagunceitor <imagem> [chave]                    demonstracao completa\n"
         "  bagunceitor encode <entrada> <saida.png> <chave>\n"
         "  bagunceitor decode <entrada.png> <saida.png> <chave>\n"
         "  bagunceitor verify <imagem1> <imagem2>\n"
         "  bagunceitor confusao|difusao|desfaz-difusao|desfaz-confusao <entrada> <saida.png> <chave>\n");
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    uso();
    exit(1);
  }
  
  static const char* comandos[] = {"encode", "decode", "confusao", "difusao",
                                   "desfaz-difusao", "desfaz-confusao"};
  for (size_t i = 0; i < sizeof comandos / sizeof comandos[0]; i++) {
    if (strcmp(argv[1], comandos[i]) == 0) {
      if (argc != 5) {
        uso();
        exit(1);
      }
      return comando(argv[1], argv[2], argv[3], argv[4]);
    }
  }
  if (strcmp(argv[1], "verify") == 0) {
    if (argc != 4) {
      uso();
      exit(1);
    }
    return verify(argv[2], argv[3]);
  }
  
  // Sem comando: modo do codigo-base, ./bagunceitor imagem [chave]
  if (argc > 3) {
    uso();
    exit(1);
  }
  return demo(argv[1], argc == 3 ? argv[2] : CHAVE_PADRAO);
}

void load(const char* name, Img* pic) {
  // Pede sempre 3 canais: se o arquivo tiver alfa (PNG RGBA) ou for
  // tons de cinza, o stb converte para RGB e o vetor de Pixel continua
  // valido. Com 0 aqui, uma imagem RGBA seria lida errado como Pixel.
  pic->pixels =
      (Pixel*)stbi_load(name, &pic->width, &pic->height, &pic->channels, 3);
  if (!pic->pixels) {
    printf("Erro de leitura (%s): %s\n", name, stbi_failure_reason());
    exit(1);
  }
  printf("Load: %s %d x %d x %d\n", name, pic->width, pic->height, pic->channels);
}