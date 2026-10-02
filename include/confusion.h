#ifndef CONFUSION_H
#define CONFUSION_H

#include <stddef.h>
#include <stdint.h>

/* Lado (em pixels) dos blocos quadrados usados pela confusao. */
#define CONFUSION_BLOCO 32

/*
 * Regra autoral de CONFUSAO: altera o VALOR das cores, sem mover nenhum
 * pixel de lugar. Granularidade = bloco de CONFUSION_BLOCO x
 * CONFUSION_BLOCO pixels: todos os pixels de um bloco recebem a mesma
 * transformacao de cor, e cada bloco recebe uma transformacao diferente.
 * Assim as formas da imagem continuam visiveis, mas as cores viram um
 * mosaico -- o efeito da confusao fica perceptivel a olho nu.
 *
 * Para cada bloco, na ordem de varredura (linha de blocos por linha de
 * blocos), o PRNG semeado pela chave sorteia:
 *   perm  = uma das 6 ordens dos canais (RGB, RBG, GRB, GBR, BRG, BGR)
 *   neg   = 3 bits: quais canais sao invertidos (negativo: 255 - v)
 *   off   = um deslocamento de 0..255 para cada canal
 * e cada canal de saida c de cada pixel do bloco vira:
 *   out[c] = (N_c(in[perm[c]]) + off[c]) mod 256
 * onde N_c(v) = 255 - v se o bit c de neg estiver ligado, ou v caso
 * contrario. A soma "mod 256" da a volta no 255 (ex.: 200 + 100 = 44),
 * o que cria contornos bem visiveis nas areas claras/escuras.
 *
 * Cada passo e uma bijecao (trocar canais, 255 - v e somar mod 256 sao
 * todos reversiveis), entao confusion_invert() desfaz tudo exatamente:
 *   t = (out[c] - off[c]) mod 256;  in[perm[c]] = N_c(t)
 *
 * 'data' tem width * height pixels de 3 bytes (R,G,B), linha a linha.
 */
void confusion_apply(uint8_t *data, int width, int height, const char *key);

/* Desfaz exatamente confusion_apply() para a mesma chave. */
void confusion_invert(uint8_t *data, int width, int height, const char *key);

#endif /* CONFUSION_H */
