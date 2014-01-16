#include <stdio.h>

#include "hp_bmap.h"

#define PRINT_EXPR(f, x)  printf("%s = " f "\n", #x, (x))


void
hp_bmap_print(struct hp_bmap * const bm)
{
  HP_BMAP_UNIT *p;
  unsigned  n;

  for (p = hp_bmap_data(bm), n = hp_bmap_data_size(bm); n; --n, ++p) {
    printf("%08x\n", *p);
  }
}

int
main(void)
{
  hp_bmap_var bm1;

  printf("----------\n");

  hp_bmap_new(bm1, 128);

  hp_bmap_print(bm1);

  printf("----------\n");

  hp_bmap_set32(bm1, 0, 5, 0x55);

  hp_bmap_print(bm1);

  printf("----------\n");

  hp_bmap_set32(bm1, 33, 8, 0xff);

  hp_bmap_print(bm1);


  PRINT_EXPR("%x", hp_bmap_get32(bm1, 1, 5));

  hp_bmap_free(bm1);

  return (0);
}

