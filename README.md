# T1 — Baixo Nível: Confusão e Difusão em Imagens (`bagunceitor`) - Eduarda Fuchs e Lucas Volkweis

Implementação em **C** de um esquema de embaralhamento e recuperação de
imagens baseado nos dois princípios clássicos de cifragem:

- **Confusão**: altera o *valor* de cada byte de cor, sem mover nada de lugar.
- **Difusão**: altera a *posição* de cada pixel na memória, sem mudar nenhum valor.

O projeto parte do **código-base da disciplina** (`main.c` com as
estruturas `Pixel`/`Img`, a função `load()` e as bibliotecas
`stb_image`/`stb_image_write`). A imagem é carregada inteira para um
vetor contíguo de `Pixel` em memória, as regras autorais leem da imagem
de entrada (`pin`) e escrevem na imagem de saída (`pout`), e o resultado
é gravado em PNG. A recuperação é **byte a byte exata**, não apenas
"parecida".

## Estrutura do projeto

```
main.c            código-base + orquestração (encode / decode / verify / etapas)
src/
  prng.c          gerador pseudoaleatório determinístico (xorshift32) semeado pela chave
  confusion.c     regra autoral de confusão + sua inversa
  diffusion.c     regra autoral de difusão + sua inversa
include/
  prng.h, confusion.h, diffusion.h
  stb_image.h, stb_image_write.h   (bibliotecas do código-base)
predio32.jpg      imagem de exemplo do código-base (1280×960)
imagens/          outras imagens de teste
relatorio/        RELATORIO.md (regras, prova de reversibilidade, uso de memória)
Makefile, CMakeLists.txt, .vscode/   (build do código-base)
```

## Compilando

Como no código-base:

- **VS Code:** `CTRL+SHIFT+B` (Windows/Linux) ou `CMD+SHIFT+B` (macOS).
- **Terminal:** `make` (Linux/macOS) ou `mingw32-make` (Windows).

## Usando

### Demonstração completa (modo do código-base)

```sh
./bagunceitor predio32.jpg                  # usa a chave padrão
./bagunceitor minha_foto.jpg "minha-chave"  # sua foto, sua chave
```

ou `make run IMG=minha_foto.jpg KEY="minha-chave"`. No VS Code, `F5`
roda com `predio32.jpg` (configurado em `.vscode/launch.json`).

A entrada pode ser **JPG, PNG, BMP** etc. O programa grava, na pasta
onde foi executado, um arquivo por etapa com o nome da imagem:

| Arquivo | O que mostra |
|---|---|
| `predio32_1_so_confusao.png` | só a confusão: cada bloco de 32×32 ganha outras cores, nada muda de lugar |
| `predio32_2_so_difusao.png` | só a difusão sobre a original: peças de 32×32 trocam de lugar (quebra-cabeça), cores intactas |
| `predio32_3_embaralhada.png` | confusão + difusão: peças com cores trocadas e fora de lugar (a imagem "bagunçada") |
| `predio32_4_recuperada.png` | decode da embaralhada, conferido byte a byte com a original |

### Comandos individuais

```sh
./bagunceitor encode predio32.jpg embaralhada.png "chave"
./bagunceitor decode embaralhada.png recuperada.png "chave"
./bagunceitor verify predio32.jpg recuperada.png
```

Cada etapa também existe isolada:

```sh
./bagunceitor confusao        entrada saida.png "chave"
./bagunceitor difusao         entrada saida.png "chave"
./bagunceitor desfaz-difusao  entrada saida.png "chave"
./bagunceitor desfaz-confusao entrada saida.png "chave"
```

**Importante:** a saída precisa ser **`.png` ou `.bmp`** (formatos sem
perda). JPG altera os bytes ao comprimir, e a imagem embaralhada não
poderia mais ser recuperada. O programa recusa saídas em `.jpg`.

`make test` roda encode → decode → verify e confere que a chave errada
**não** recupera a imagem (Linux/macOS).

## As regras autorais

### Confusão — `src/confusion.c`
A imagem é dividida em **blocos de 32×32 pixels**. Para cada bloco, o
PRNG semeado pela chave sorteia uma transformação de cor, aplicada a
todos os pixels daquele bloco:

1. **troca a ordem dos canais** (uma das 6: RGB, RBG, GRB, GBR, BRG, BGR);
2. **inverte** alguns canais, como um negativo (`255 - v`);
3. **soma um deslocamento** de 0 a 255 a cada canal, dando a volta no 255
   (`mod 256`).

```
out[c] = ( N_c( in[perm[c]] ) + off[c] ) mod 256
```

Como cada bloco recebe uma transformação diferente, a imagem vira um
mosaico de cores, mas as **formas continuam visíveis**: o efeito da
confusão aparece a olho nu. Trocar canais, `255 - v` e somar `mod 256`
são todos reversíveis, e a inversa é:

```
t = ( out[c] - off[c] ) mod 256
in[perm[c]] = N_c(t)
```

### Difusão — `src/diffusion.c`
A imagem é cortada em **peças de 32×32 pixels**, como um quebra-cabeça,
e as peças são embaralhadas com Fisher-Yates, alimentado pelo mesmo PRNG
(com outro *salt*). Dentro de cada peça os pixels continuam na mesma
ordem, então dá para reconhecer pedaços da imagem fora do lugar: o
efeito da difusão aparece a olho nu.

Se a largura ou a altura não forem múltiplas de 32, sobra uma faixa na
direita e/ou embaixo que não forma peças inteiras. Os pixels dessa sobra
são embaralhados um a um, entre si.

Essa permutação **não é salva em disco**: é recalculada
deterministicamente a partir da chave tanto para embaralhar quanto para
recuperar. Essa é a "regra de mapeamento reverso" exigida pelo
enunciado.

Como a confusão e a difusão usam blocos do mesmo tamanho e alinhados, a
imagem embaralhada mostra peças inteiras, cada uma com suas cores
trocadas, fora do lugar.

### Orquestração — `main.c`
- `encode` = difusão( confusão( imagem ) )
- `decode` = confusão⁻¹( difusão⁻¹( imagem ) )

(ordem invertida em relação ao encode, como uma pilha — LIFO)

Veja `relatorio/RELATORIO.md` para a explicação completa, com prova de
reversibilidade e detalhamento do uso de memória.
