#include "confusion.h"
#include "prng.h"

#define CONFUSION_SALT 0x43554E46u /* 'CUNF' - salt exclusivo da confusao */

/* As 6 ordens possiveis dos canais: saida c recebe o canal PERMS[p][c]. */
static const uint8_t PERMS[6][3] = {
    {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
};

typedef struct {
    const uint8_t *perm; /* ordem dos canais */
    uint8_t neg;         /* bit c ligado = canal c de saida e invertido */
    uint8_t off[3];      /* deslocamento somado a cada canal (mod 256) */
} Transformacao;

/* Sorteia a transformacao do proximo bloco. Aplicar e desfazer chamam
 * esta funcao na mesma ordem, com a mesma semente, e por isso obtem
 * exatamente as mesmas transformacoes para cada bloco. */
static Transformacao sorteia(Prng *rng) {
    Transformacao t;
    uint32_t a = prng_next_u32(rng);
    uint32_t b = prng_next_u32(rng);
    t.perm = PERMS[a % 6u];
    t.neg = (uint8_t)((a >> 8) & 7u);
    t.off[0] = (uint8_t)(b & 0xFFu);
    t.off[1] = (uint8_t)((b >> 8) & 0xFFu);
    t.off[2] = (uint8_t)((b >> 16) & 0xFFu);
    return t;
}

static void aplica_pixel(uint8_t *px, const Transformacao *t) {
    uint8_t in[3] = {px[0], px[1], px[2]};
    for (int c = 0; c < 3; c++) {
        uint8_t v = in[t->perm[c]];
        if (t->neg & (1u << c)) v = (uint8_t)(255u - v);
        px[c] = (uint8_t)(v + t->off[c]); /* mod 256 */
    }
}

static void desfaz_pixel(uint8_t *px, const Transformacao *t) {
    uint8_t out[3] = {px[0], px[1], px[2]};
    for (int c = 0; c < 3; c++) {
        uint8_t v = (uint8_t)(out[c] - t->off[c]); /* mod 256 */
        if (t->neg & (1u << c)) v = (uint8_t)(255u - v);
        px[t->perm[c]] = v;
    }
}

/* Percorre os blocos em ordem de varredura e aplica 'op' em cada pixel
 * de cada bloco, com a transformacao sorteada para aquele bloco. Os
 * blocos da ultima linha/coluna podem ser menores que CONFUSION_BLOCO. */
static void percorre(uint8_t *data, int width, int height, const char *key,
                     void (*op)(uint8_t *, const Transformacao *)) {
    Prng rng;
    prng_seed(&rng, prng_seed_from_key(key, CONFUSION_SALT));

    for (int by = 0; by < height; by += CONFUSION_BLOCO) {
        for (int bx = 0; bx < width; bx += CONFUSION_BLOCO) {
            Transformacao t = sorteia(&rng);
            int ymax = by + CONFUSION_BLOCO < height ? by + CONFUSION_BLOCO : height;
            int xmax = bx + CONFUSION_BLOCO < width ? bx + CONFUSION_BLOCO : width;
            for (int y = by; y < ymax; y++) {
                for (int x = bx; x < xmax; x++) {
                    op(data + ((size_t)y * (size_t)width + (size_t)x) * 3, &t);
                }
            }
        }
    }
}

void confusion_apply(uint8_t *data, int width, int height, const char *key) {
    percorre(data, width, height, key, aplica_pixel);
}

void confusion_invert(uint8_t *data, int width, int height, const char *key) {
    percorre(data, width, height, key, desfaz_pixel);
}
