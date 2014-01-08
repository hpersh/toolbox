#include "hp_bmap_cfg.h"
#include "common/hp_common.h"

struct hp_bmap {
  unsigned     size;	/* In bits */
  HP_BMAP_UNIT *data;
};

struct hp_bmap *hp_bmap_alloc(const struct hp_bmap *bm, unsigned size);
void           hp_bmap_free(const struct hp_bmap *bm);
struct hp_bmap *hp_bmap_clear_all(const struct hp_bmap *bm);
struct hp_bmap *hp_bmap_new(const struct hp_bmap *bm, unsigned size);
struct hp_bmap *hp_bmap_get(const struct hp_bmap *to, unsigned ofs, unsigned len, const struct hp_bmap *from);
uint32         hp_bmap_get32(const struct hp_bmap *from, unsigned ofs, unsigned len);
uint64         hp_bmap_get64(const struct hp_bmap *from, unsigned ofs, unsigned len);
struct hp_bmap *hp_bmap_set(const struct hp_bmap *to, unsigned ofs, unsigned len, const struct hp_bmap *from);
struct hp_bmap *hp_bmap_set32(const struct hp_bmap *to, unsigned ofs, unsigned len, uint32 val);
struct hp_bmap *hp_bmap_set64(const struct hp_bmap *to, unsigned ofs, unsigned len, uint64 val);
HP_BMAP_UNIT   *hp_bmap_data(const struct hp_bmap *bm);
unsigned       hp_bmap_data_size(const struct hp_bmap *bm);











