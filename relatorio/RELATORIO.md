# Relatório — Confusão e Difusão em Imagens (Baixo Nível)

**Repositório GitHub:** _(https://github.com/lucasvolkweis04/pnbtrabalho1.git)_
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

## 4. Uso de memória

Toda a manipulação acontece em cima de vetores alocados com
`malloc`/`calloc` e liberados com `free`, sem escrever/ler arquivos
intermediários entre os passos de confusão e difusão:

- `load()` (via `stbi_load`) aloca um único vetor de
  `largura × altura` pixels (3 bytes cada) para a imagem de entrada.
- `nova_img()` aloca a imagem de saída com as mesmas dimensões, como o
  `out` do código-base. `encode`/`decode` usam ainda uma imagem
  intermediária do mesmo tamanho entre as duas etapas.
- `confusion_apply`/`confusion_invert` operam **in-place** no vetor de
  saída, bloco a bloco (não alocam memória extra).
- `diffusion_apply`/`diffusion_invert` leem de `pin` e escrevem em
  `pout` (uma permutação geral precisa de dois vetores). Eles alocam o
  vetor `dest` (`largura × altura` valores de `size_t`), o vetor das
  peças e os vetores da sobra, e liberam todos antes de retornar.
- `stbi_write_png` grava o vetor final direto para o arquivo, e todos os
  vetores são liberados com `free` ao final.

Testado com `make test` e com AddressSanitizer/UBSan (imagens JPG, PNG
com canal alfa, tons de cinza, BMP e 1×1), sem erros de memória.

