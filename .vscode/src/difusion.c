#include "diffusion.h"
#include "prng.h"

#include <stdlib.h>
#include <string.h>

#define DIFFUSION_SALT 0x44494646u /* 'DIFF' - salt exclusivo da difusao, diferente da confusao */
#define BYTES_PER_PIXEL 3

/*
 * Monta dest[i] = posicao final do pixel i, para todos os pixels:
 *  - pixels dentro de pecas inteiras andam junto com a sua peca, mantendo
 *    o mesmo deslocamento (dx, dy) dentro dela;
 *  - pixels da sobra (faixa direita/inferior) vao para outra posicao da
 *    propria sobra.
 */
static size_t *build_destinations(int width, int height, const char *key) {
    const int B = DIFFUSION_BLOCO;
    size_t w = (size_t)width, h = (size_t)height;
    size_t bw = w / B, bh = h / B;       /* pecas inteiras por linha/coluna */
    size_t n_pecas = bw * bh;
    size_t n_pixels = w * h;
    size_t n_sobra = n_pixels - n_pecas * B * B;

    size_t *dest = (size_t *)malloc((n_pixels ? n_pixels : 1) * sizeof(size_t));
    size_t *pecas = (size_t *)malloc((n_pecas ? n_pecas : 1) * sizeof(size_t));
    size_t *sobra = (size_t *)malloc((n_sobra ? n_sobra : 1) * sizeof(size_t));
    size_t *perm_sobra = (size_t *)malloc((n_sobra ? n_sobra : 1) * sizeof(size_t));
    if (!dest || !pecas || !sobra || !perm_sobra) {
        free(dest); free(pecas); free(sobra); free(perm_sobra);
        return NULL;
    }

    Prng rng;
    prng_seed(&rng, prng_seed_from_key(key, DIFFUSION_SALT));
    prng_fisher_yates(&rng, pecas, n_pecas);       /* peca k vai para pecas[k] */
    prng_fisher_yates(&rng, perm_sobra, n_sobra);  /* mesma sequencia, continuada */

    /* Lista, em ordem de varredura, os pixels que ficaram fora das pecas. */
    size_t s = 0;
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            if (x >= bw * B || y >= bh * B) sobra[s++] = y * w + x;
        }
    }

    s = 0;
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            size_t i = y * w + x;
            if (x < bw * B && y < bh * B) {
                size_t k = (y / B) * bw + (x / B);   /* peca de origem */
                size_t d = pecas[k];                 /* peca de destino */
                size_t dx = x % B, dy = y % B;       /* posicao dentro da peca */
                dest[i] = ((d / bw) * B + dy) * w + (d % bw) * B + dx;
            } else {
                dest[i] = sobra[perm_sobra[s++]];
            }
        }
    }

    free(pecas);
    free(sobra);
    free(perm_sobra);
    return dest;
}

int diffusion_apply(const uint8_t *src, uint8_t *dst, int width, int height, const char *key) {
    size_t n = (size_t)width * (size_t)height;
    if (n == 0) return 0;
    size_t *dest = build_destinations(width, height, key);
    if (!dest) return -1;

    /* scatter: pixel de origem i vai para o destino dest[i] */
    for (size_t i = 0; i < n; i++) {
        memcpy(dst + dest[i] * BYTES_PER_PIXEL,
               src + i * BYTES_PER_PIXEL,
               BYTES_PER_PIXEL);
    }

    free(dest);
    return 0;
}

int diffusion_invert(const uint8_t *src, uint8_t *dst, int width, int height, const char *key) {
    size_t n = (size_t)width * (size_t)height;
    if (n == 0) return 0;
    size_t *dest = build_destinations(width, height, key);
    if (!dest) return -1;

    /* gather: pixel i volta a ser o que foi levado para dest[i] */
    for (size_t i = 0; i < n; i++) {
        memcpy(dst + i * BYTES_PER_PIXEL,
               src + dest[i] * BYTES_PER_PIXEL,
               BYTES_PER_PIXEL);
    }

    free(dest);
    return 0;
}
