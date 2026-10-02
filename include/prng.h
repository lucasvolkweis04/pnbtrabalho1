#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>
#include <stddef.h>

/*
 * Gerador pseudoaleatorio determinístico (xorshift32). Dada a mesma
 * semente, produz sempre a mesma sequencia de numeros -- e exatamente
 * essa propriedade que permite reconstruir, na hora de decodificar, a
 * mesma sequencia de bytes/permutacoes usada para codificar.
 */
typedef struct {
    uint32_t state;
} Prng;

/* Inicializa o gerador a partir de uma semente de 32 bits (nunca zero). */
void prng_seed(Prng *rng, uint32_t seed);

/* Deriva uma semente de 32 bits a partir de uma chave textual (FNV-1a). */
uint32_t prng_seed_from_key(const char *key, uint32_t salt);

/* Proximo numero pseudoaleatorio de 32 bits da sequencia. */
uint32_t prng_next_u32(Prng *rng);

/* Embaralha 'arr' (indices 0..n-1) com Fisher-Yates usando 'rng'.
 * A sequencia de trocas depende apenas de n e da semente do gerador,
 * nunca do conteudo de 'arr' -- por isso pode ser reproduzida
 * exatamente a partir da mesma chave, sem precisar guardar a permutacao. */
void prng_fisher_yates(Prng *rng, size_t *arr, size_t n);

#endif /* PRNG_H */
