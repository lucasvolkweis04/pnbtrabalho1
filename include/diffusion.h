#ifndef DIFFUSION_H
#define DIFFUSION_H

#include <stddef.h>
#include <stdint.h>

/* Lado (em pixels) das pecas quadradas movidas pela difusao. */
#define DIFFUSION_BLOCO 32

/*
 * Regra autoral de DIFUSAO: muda a POSICAO dos pixels, sem alterar
 * nenhum valor de cor. Granularidade = bloco de DIFFUSION_BLOCO x
 * DIFFUSION_BLOCO pixels: a imagem e cortada em pecas quadradas, como um
 * quebra-cabeca, e as pecas sao embaralhadas. Dentro de cada peca os
 * pixels continuam na mesma ordem, entao da para reconhecer pedacos da
 * imagem fora do lugar -- o efeito da difusao fica perceptivel a olho nu.
 *
 * Se a largura ou a altura nao forem multiplas de DIFFUSION_BLOCO, sobra
 * uma faixa na direita e/ou embaixo que nao forma pecas inteiras. Os
 * pixels dessa sobra sao embaralhados um a um, entre si.
 *
 * As duas permutacoes (das pecas e da sobra) sao geradas com Fisher-Yates
 * a partir do PRNG semeado pela chave (ver prng.c). Elas NAO sao
 * armazenadas em disco: sao recalculadas deterministicamente a partir da
 * mesma chave tanto para codificar quanto para decodificar, o que
 * comprova o "mapeamento reverso" exigido pelo enunciado.
 *
 * Seguindo o modelo do codigo-base (imagem de entrada -> imagem de saida),
 * as duas funcoes leem de 'src' e escrevem em 'dst', que devem ser
 * buffers DIFERENTES de width * height pixels de 3 bytes. Sendo dest(i)
 * a posicao para onde vai o pixel i:
 *   diffusion_apply  (scatter): dst[dest(i)] = src[i]
 *   diffusion_invert (gather):  dst[i]       = src[dest(i)]
 * Como dest e uma bijecao, gather(scatter(x)) == x sempre, byte a byte.
 *
 * Retornam 0 em caso de sucesso e -1 se faltar memoria para a
 * permutacao (nesse caso 'dst' nao deve ser usado).
 */
int diffusion_apply(const uint8_t *src, uint8_t *dst, int width, int height, const char *key);
int diffusion_invert(const uint8_t *src, uint8_t *dst, int width, int height, const char *key);

#endif /* DIFFUSION_H */
