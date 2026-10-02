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