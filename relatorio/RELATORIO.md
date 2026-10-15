# Relatório — Confusão e Difusão em Imagens (Baixo Nível)

**Repositório GitHub:** _(https://github.com/lucasvolkweis04/pnbtrabalho1.git)_
**Vídeo Youtube Não Listado** _(https://youtu.be/ujaqXGNqfJA)_
**Membros do grupo:** _(Eduarda Fuchs e Lucas Volkweis)_

## 1. A imagem digital na memória

O programa parte do código-base da disciplina (`main.c`). A função
`load()` usa a biblioteca `stb_image` para ler a imagem (JPG, PNG, BMP…)
e devolve um único vetor contíguo de `Pixel`:

```c
typedef struct { unsigned char r, g, b; } Pixel;   // 3 bytes, 0–255 cada
typedef struct { int width, height, channels; Pixel* pixels; } Img;
```

Os pixels ficam em ordem de varredura, linha a linha, de cima para
baixo: o pixel da linha `i`, coluna `j` está em `pixels[i * width + j]`.
Não há cabeçalho nem *padding* de linha nesse vetor, porque a biblioteca
já decodificou o formato do arquivo. `load()` pede sempre 3 canais à
`stb_image`: se o arquivo tiver canal alfa (PNG RGBA) ou for em tons de
cinza, ele é convertido para RGB, e o vetor de `Pixel` continua válido.
Um `_Static_assert` garante que `sizeof(Pixel) == 3`, isto é, que o
vetor pode ser tratado como uma sequência de bytes R,G,B,R,G,B…

Esse vetor é o "papel" sobre o qual as duas transformações abaixo atuam
diretamente, em memória, sem arquivos temporários. O resultado é gravado
com `stb_image_write` em **PNG** (ou BMP), formatos sem perda. Gravar em
JPG não é permitido: a compressão com perda mudaria os bytes da imagem
embaralhada, e a recuperação exata deixaria de ser possível.

## 2. Regras autorais

### 2.1 Confusão (`src/confusion.c`)

**Ideia:** alterar o *valor* das cores, mantendo cada pixel na sua
posição, de um jeito que o efeito seja **perceptível a olho nu**.

**Granularidade:** bloco de 32×32 pixels (`CONFUSION_BLOCO`). Todos os
pixels de um bloco recebem a mesma transformação, e cada bloco recebe uma
transformação diferente. Os blocos da última linha e da última coluna
podem ser menores, quando a largura ou a altura não são múltiplas de 32.

Um gerador pseudoaleatório determinístico (xorshift32, `src/prng.c`) é
semeado a partir de um hash FNV-1a da chave. Percorrendo os blocos em
ordem de varredura, para cada bloco:

1. tiram-se do PRNG dois números `a` e `b` de 32 bits;
2. `perm = PERMS[a % 6]` → uma das 6 ordens dos canais R, G, B;
3. `neg = (a >> 8) & 7` → 3 bits dizendo quais canais de saída são
   invertidos (negativo, `255 - v`);
4. `off[0..2]` = os 3 bytes baixos de `b` → deslocamento de cada canal;
5. cada canal de saída `c` de cada pixel do bloco vira

```
out[c] = ( N_c( in[perm[c]] ) + off[c] ) mod 256
N_c(v) = 255 - v   se o bit c de neg estiver ligado
       = v         caso contrário
```

A soma `mod 256` "dá a volta" no 255 (por exemplo, 200 + 100 = 44), o
que cria contornos bem marcados nas regiões claras e escuras. O
resultado é um mosaico de blocos coloridos em que as formas da imagem
original (prédio, janelas, carros) continuam reconhecíveis.

**Prova de reversibilidade:** cada passo é uma bijeção em [0, 255]:
somar `off` mod 256 se desfaz subtraindo `off` mod 256; `255 - v` é sua
própria inversa; e trocar os canais pela permutação `perm` se desfaz
devolvendo cada valor ao canal `perm[c]`. Aplicando os passos na ordem
inversa:

```
t = ( out[c] - off[c] ) mod 256
  = N_c( in[perm[c]] )
N_c(t) = N_c( N_c( in[perm[c]] ) ) = in[perm[c]]
```

`confusion_invert()` percorre os blocos na mesma ordem, com a mesma
semente, e por isso sorteia exatamente as mesmas `(perm, neg, off)` para
cada bloco antes de aplicar essa fórmula inversa.

**Troca consciente:** essa regra é mais fácil de visualizar, mas menos
segura que uma transformação diferente por byte. Todos os pixels de um
bloco compartilham a mesma transformação, então quem conhece um pixel
original do bloco descobre a transformação do bloco inteiro. Na
imagem final, as peças também trocam de lugar, o que dificulta saber de
qual parte da foto cada bloco veio.

### 2.2 Difusão (`src/diffusion.c`)

**Ideia:** mudar a *posição* dos pixels, sem alterar nenhum valor, de um
jeito que o efeito seja **perceptível a olho nu**.

**Granularidade:** peça de 32×32 pixels (`DIFFUSION_BLOCO`). A imagem é
cortada em peças quadradas, como um quebra-cabeça, e as peças trocam de
lugar entre si. Dentro de cada peça os pixels continuam na mesma ordem,
então a imagem só com difusão mostra pedaços reconhecíveis da foto
(janelas, céu, carros) fora de ordem.

Com `bw = largura / 32` e `bh = altura / 32` peças inteiras por linha e
por coluna:

1. um vetor `pecas` de `bw × bh` posições é embaralhado com
   **Fisher-Yates**, alimentado pelo PRNG semeado com a chave e um *salt*
   diferente do da confusão (`DIFFUSION_SALT` ≠ `CONFUSION_SALT`). A peça
   `k` vai para a posição `pecas[k]`;
2. um pixel `(x, y)` dentro da peça `k` vai para a peça `pecas[k]`, no
   mesmo deslocamento `(x mod 32, y mod 32)` dentro dela;
3. se a largura ou a altura não forem múltiplas de 32, sobra uma faixa à
   direita e/ou embaixo que não forma peças inteiras. Os pixels dessa
   sobra são embaralhados um a um entre si, com um segundo Fisher-Yates
   que continua a mesma sequência do PRNG.

Isso define, para cada pixel `i`, a posição de destino `dest(i)`.

Essa permutação **nunca é gravada em disco**, nem junto com a imagem
embaralhada nem em nenhum outro arquivo. Ela é recalculada de forma
100% determinística a partir da chave e das dimensões da imagem, tanto
ao embaralhar quanto ao recuperar. Esse é o "mapeamento reverso" exigido
pelo enunciado.

- **Aplicar (scatter):** `pout[dest(i)] = pin[i]` para todo `i`.
- **Inverter (gather):** `pout[i] = pin[dest(i)]` para todo `i`.

**Prova de reversibilidade:** `dest` é uma *bijeção* de
`[0, largura × altura)` nela mesma. O Fisher-Yates das peças é uma
bijeção entre as peças; o deslocamento dentro da peça é preservado; e o
Fisher-Yates da sobra é uma bijeção entre os pixels da sobra. As peças e
a sobra não se misturam, então toda posição de destino é usada
exatamente uma vez. Logo `gather` é a função inversa de `scatter`: para
cada `i`, o pixel que `scatter` colocou em `dest(i)` é exatamente o que
`gather` lê de volta para a posição `i`.

**Troca consciente:** como na confusão, a regra ficou mais visível e
menos segura. Dentro de cada peça a imagem continua intacta, então dá
para montar o quebra-cabeça olhando as bordas das peças, sem a chave.

### 2.3 Orquestração (`main.c`)

Seguindo o modelo do código-base, cada etapa lê de uma imagem de
entrada (`pin`) e escreve em uma imagem de saída (`pout`):

```
encode(in, out, chave):
    tmp = confusao(in, chave)          # 1º: mexe nos valores
    out = difusao(tmp, chave)          # 2º: mexe nas posições

decode(in, out, chave):
    tmp = desfaz_difusao(in, chave)    # desfaz o 2º passo primeiro
    out = desfaz_confusao(tmp, chave)  # depois desfaz o 1º passo
```

A ordem de `decode` é o inverso exato da ordem de `encode` (como uma
pilha/LIFO) — é isso que garante que a composição das duas
transformações também seja perfeitamente reversível, e não só cada uma
isoladamente.

Para facilitar a visualização, `./bagunceitor imagem [chave]` grava
cada etapa separadamente: só confusão, só difusão, as duas juntas
(embaralhada) e a recuperada.

## 3. Recuperação exata (prova)

O comando `./bagunceitor verify` compara duas imagens **byte a byte**
nos dados de pixel (não apenas visualmente), reportando o número exato
de bytes divergentes. `make test` automatiza o ciclo completo:

```
encode(predio32.jpg, chave) → decode(chave) → verify → decode(chave errada) → verify
```

Resultado observado com a imagem `predio32.jpg` do código-base
(1280×960, 3.686.400 bytes de pixel) e a chave `"senha-do-grupo-123"`:

| Comparação                                           | Bytes divergentes |
|------------------------------------------------------|--------------------|
| original × embaralhada (`encode`)                     | 3.672.210 / 3.686.400 (≈99,6%) |
| original × recuperada com a chave **correta**         | **0 / 3.686.400 (idênticas)** |
| original × recuperada com a chave errada `"senha-errada"` | 3.671.187 / 3.686.400 (≈99,6%) |

Isso demonstra três coisas ao mesmo tempo: (1) o embaralhamento de fato
transforma quase todos os bytes, (2) a chave correta recupera a imagem
**exatamente**, byte a byte, e (3) sem a chave correta não há
recuperação — a chave é indispensável tanto para desfazer a confusão
quanto a difusão, já que ambas derivam sua sequência pseudoaleatória
dela.

## 4. Uso de memória: ponteiros e aritmética

Toda a manipulação ocorre em vetores contíguos alocados com `malloc`/`calloc`
e liberados com `free`, sem arquivos intermediários entre as etapas.

### 4.1 Vetor de `Pixel` visto como bytes (`main.c`)

```c
unsigned char* pin  = (unsigned char*)in->pixels;
unsigned char* pout = (unsigned char*)out->pixels;
memcpy(pout, pin, n * sizeof(Pixel));
```

`stbi_load` devolve os pixels em um único bloco. O *cast* para
`unsigned char*` permite tratá-lo como a sequência R,G,B,R,G,B…, e o
`_Static_assert(sizeof(Pixel) == 3)` garante que não há *padding*.

### 4.2 Endereço de um pixel por aritmética de ponteiros (`confusion.c`)

```c
op(data + ((size_t)y * (size_t)width + (size_t)x) * 3, &t);
```

O pixel (x, y) começa em `data + (y*largura + x) * 3`. O ponteiro `px`
recebido aponta para o primeiro byte do pixel, e `px[0]`, `px[1]`, `px[2]`
(equivalentes a `*(px+0)`, `*(px+1)`, `*(px+2)`) são R, G e B. Os blocos
são percorridos por varredura, e a função de cada pixel (`aplica_pixel` ou
`desfaz_pixel`) é passada por **ponteiro para função**, de modo que a mesma
varredura serve para aplicar e para desfazer.

A tabela `PERMS[6][3]` é lida por ponteiro: `t.perm = PERMS[a % 6u]`
guarda o endereço da linha sorteada, e `in[t->perm[c]]` indexa o canal.

### 4.3 Mover pixels pela memória (`diffusion.c`)

```c
/* aplicar (scatter) */
memcpy(dst + dest[i] * BYTES_PER_PIXEL, src + i * BYTES_PER_PIXEL, BYTES_PER_PIXEL);
/* desfazer (gather) */
memcpy(dst + i * BYTES_PER_PIXEL, src + dest[i] * BYTES_PER_PIXEL, BYTES_PER_PIXEL);
```

O vetor `dest` (um `size_t` por pixel, alocado com `malloc`) guarda o
destino de cada pixel. A posição final dentro de uma peça é calculada por
aritmética de índices:

```c
dest[i] = ((d / bw) * B + dy) * w + (d % bw) * B + dx;
```

em que `d` é a peça de destino, `(dx, dy)` o deslocamento dentro da peça e
`w` a largura. Aplicar e desfazer diferem só em qual lado leva o `dest[i]`.
O `src` é lido e o `dst` é escrito em vetores diferentes, pois uma
permutação geral não pode ser feita sem cópia.

### 4.4 Ponteiro percorrendo a chave (`prng.c`)

```c
for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
    hash ^= *p;
    hash *= 16777619u;
}
```

O ponteiro `p` avança byte a byte pela *string* da chave até o `'\0'`.

### 4.5 Alocação e liberação

| Onde | Alocação | Liberação |
|---|---|---|
| `load()` | `stbi_load` (imagem de entrada) | `free(in.pixels)` |
| `nova_img()` | `calloc` (saída e intermediária) | `free(...)` ao fim de cada comando |
| `build_destinations()` | `dest`, `pecas`, `sobra`, `perm_sobra` | os três auxiliares com `free` dentro da função; `dest` após o uso |

`confusion_*` opera *in-place* e não aloca. 

## Uso de IA

Conforme exigido no enunciado, declaramos o uso de IA neste trabalho:

- **Documentação e organização:** usamos IA (Claude) para estruturar e redigir o `README.md` e este relatório em Markdown, para organizar o trabalho (roteiro, checklist de requisitos) e para diagnosticar o erro de compilação (Makefile).
