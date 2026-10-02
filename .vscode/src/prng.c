#include "prng.h"

void prng_seed(Prng *rng, uint32_t seed) {
    rng->state = (seed == 0) ? 0x9E3779B9u : seed; /* xorshift nao pode ter estado 0 */
}

uint32_t prng_seed_from_key(const char *key, uint32_t salt) {
    /* FNV-1a de 32 bits sobre os bytes da chave, com um "salt" extra
     * misturado para permitir gerar duas sementes independentes (uma
     * para confusao, outra para difusao) a partir da mesma chave. */
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    hash ^= salt;
    hash *= 16777619u;
    if (hash == 0) hash = 0x9E3779B9u;
    return hash;
}

uint32_t prng_next_u32(Prng *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

void prng_fisher_yates(Prng *rng, size_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) arr[i] = i;
    if (n < 2) return;
    for (size_t i = n - 1; i > 0; i--) {
        uint32_t r = prng_next_u32(rng);
        size_t j = (size_t)(r % (uint32_t)(i + 1));
        size_t tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}
