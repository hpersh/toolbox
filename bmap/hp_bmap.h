#include "common/hp_common.h"

#define HP_BMAP_UNIT               uint32
#define HP_BMAP_UNIT_BITS_LOG2     5
#define HP_BMAP_UNIT_BITS          (1 << HP_BMAP_UNIT_BITS_LOG2)
#define HP_BMAP_BIT_IDX_TO_UNIT_IDX(i)  ((i) >> HP_BMAP_UNIT_BITS_LOG2)
#define HP_BMAP_BIT_IDX_TO_UNIT_SH(i)   ((i) & (HP_BMAP_UNIT_BITS - 1))
#define HP_BMAP_BITS_TO_UNITS(n)   (HP_BMAP_BIT_IDX_TO_UNIT_IDX((n) - 1) + 1)
#define HP_BMAP_UNITS_TO_BYTES(n)  ((n) * sizeof(HP_BMAP_UNIT))

struct hp_bmap {
  unsigned     size;	/* In bits */
  HP_BMAP_UNIT *data;
};

typedef struct hp_bmap hp_bmap[1];

hp_bmap_p    hp_bmap_alloc(const hp_bmap_p bm, unsigned size);
void         hp_bmap_free(const hp_bmap_p bm);
hp_bmap_p    hp_bmap_clear_all(const hp_bmap_p bm);
hp_bmap_p    hp_bmap_new(const hp_bmap_p bm, unsigned size);
hp_bmap_p    hp_bmap_get(const hp_bmap_p to, unsigned ofs, unsigned len, const hp_bmap_p from);
uint32       hp_bmap_get32(const hp_bmap_p from, unsigned ofs, unsigned len);
uint64       hp_bmap_get64(const hp_bmap_p from, unsigned ofs, unsigned len);
hp_bmap_p    hp_bmap_set(const hp_bmap_p to, unsigned ofs, unsigned len, const hp_bmap_p from);
hp_bmap_p    hp_bmap_set32(const hp_bmap_p to, unsigned ofs, unsigned len, uint32 val);
hp_bmap_p    hp_bmap_set64(const hp_bmap_p to, unsigned ofs, unsigned len, uint64 val);
HP_BMAP_UNIT *hp_bmap_data(const hp_bmap_p bm);
unsigned     hp_bmap_data_size(const hp_bmap_p bm);











