#include <stdlib.h>
#include <string.h>

#include "hp_bmap.h"

#ifdef __UNIT_TEST__
#include <assert.h>
#define ASSERT  assert
#else
#include "assert/hp_assert.h"
#define ASSERT  hp_assert
#endif


hp_bmap_p
hp_bmap_alloc(const hp_bmap_p bm, unsigned size)
{
  struct hp_bmap *_bm = (struct hp_bmap *) bm;

  _bm->size = size;
  _bm->data = (HP_BMAP_UNIT *) hp_mem_alloc(HP_BMAP_UNITS_TO_BYTES(HP_BMAP_BITS_TO_UNITS(_bm->size)));

  return (bm);
}

void
hp_bmap_free(const hp_bmap_p bm)
{
  struct hp_bmap *_bm = (struct hp_bmap *) bm;

  if (_bm->data)  free((void *) _bm->data);
  
  _bm->size = 0;
  _bm->data = 0;
}


static void
bmap_clear_all(unsigned size, HP_BMAP_UNIT * const data)
{
    memset(data, 0, HP_BMAP_UNITS_TO_BYTES(HP_BMAP_BITS_TO_UNITS(size)));
}


hp_bmap_p
hp_bmap_clear_all(const hp_bmap_p bm)
{
    bmap_clear_all(bm->size, bm->data);

    return (bm);
}

struct hp_bmap *
hp_bmap_new(struct hp_bmap * const bm, unsigned size)
{
    return (hp_bmap_clear_all(hp_bmap_alloc(bm, size)));
}


static void
bmap_get(unsigned to_size, HP_BMAP_UNIT * const to_data, unsigned ofs, unsigned len, unsigned from_size, const HP_BMAP_UNIT * const from_data)
{
    unsigned  sh, r;
    HP_BMAP_UNIT *p;

    ASSERT((ofs + len) <= from_size);
    ASSERT(len <= to_size);

    bmap_clear_all(to_size, to_data);

    sh = HP_BMAP_BIT_IDX_TO_UNIT_SH(ofs);
    r  = HP_BMAP_UNIT_BITS - sh;
    for (p = from_data + HP_BMAP_BIT_IDX_TO_UNIT_IDX(ofs); ; len -= HP_BMAP_UNIT_BITS, ++p, ++to_data) {
        *to_data = p[0] >> sh;
        if (len > r)  *to_data |= p[1] << r;
        if (len <= HP_BMAP_UNIT_BITS) {
            *to_data &= (1 << len) - 1;
            break;
        }
    }
}

struct hp_bmap *
hp_bmap_get(const struct hp_bmap * const to, unsigned ofs, unsigned len, const struct hp_bmap * const from)
{
    bmap_get(to->size, to->data, ofs, len, from->size, from->data);

    return (to);
}

uint32
hp_bmap_get32(const struct hp_bmap * const from, unsigned ofs, unsigned len)
{
    HP_BMAP_UNIT temp[1];

    bmap_get(sizeof(temp) << 3, temp, ofs, len, from->size, from->data);

    return (temp[0]);
}

uint64
hp_bmap_get64(const struct hp_bmap * const from, unsigned ofs, unsigned len)
{
    HP_BMAP_UNIT temp[2];

    bmap_get(sizeof(temp) << 3, temp, ofs, len, from->size, from->data);

    return ((((uint64) temp[1]) << 32) | temp[0]);
}


static void
bmap_set(unsigned to_size, HP_BMAP_UNIT * const to_data, unsigned ofs, unsigned len, unsigned from_size, const HP_BMAP_UNIT * const from_data)
{
    unsigned  sh, r, b;
    HP_BMAP_UNIT *p, m, u;

    ASSERT((ofs + len) <= to_size);
    ASSERT(len <= from_size);

    sh = HP_BMAP_BIT_IDX_TO_UNIT_SH(ofs);
    r  = HP_BMAP_UNIT_BITS - sh;
    for (p = to_data + HP_BMAP_BIT_IDX_TO_UNIT_IDX(ofs); len; len -= b, ++p, ++from_data) {
        b = len > HP_BMAP_UNIT_BITS ? HP_BMAP_UNIT_BITS : len;
        m = (1 << b) - 1;
        u = *from_data & m;
        p[0] = (p[0] & ~(m << sh)) | (u << sh);
        if (b > r)  p[1] = (p[1] & ~(m >> r)) | (u >> r);
    }
}

struct hp_bmap *
hp_bmap_set(const struct hp_bmap * const to, unsigned ofs, unsigned len, const struct hp_bmap * const from)
{
    bmap_set(to->size, to->data, ofs, len, from->size, from->data);

    return (to);
}

struct hp_bmap *
hp_bmap_set32(const struct hp_bmap * const to, unsigned ofs, unsigned len, uint32 val)
{
    HP_BMAP_UNIT temp[1];

    temp[0] = val;
    bmap_set(to->size, to->data, ofs, len, sizeof(temp) << 3, temp);

    return (to);
}

struct hp_bmap *
hp_bmap_set64(const struct hp_bmap * const to, unsigned ofs, unsigned len, uint64 val)
{
    HP_BMAP_UNIT temp[2];

    temp[0] = val;
    temp[1] = val >> HP_BMAP_UNIT_BITS;
    bmap_set(to->size, to->data, ofs, len, sizeof(temp) << 3, temp);

    return (to);
}

HP_BMAP_UNIT *
hp_bmap_data(const struct hp_bmap * const bm)
{
  return (bm->data);
}

unsigned
hp_bmap_data_size(const struct hp_bmap * const bm)
{
  return (HP_BMAP_BITS_TO_UNITS(bm->size));
}


#ifdef __UNIT_TEST__

#include <stdio.h>

#define PRINT_EXPR(f, x)  printf("%s = " f "\n", #x, (x))

void
hp_bmap_print(const struct hp_bmap * const bm)
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
  hp_bmap bm1;

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

#endif
