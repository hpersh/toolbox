#include "hp_bmap_cfg.h"
#include "common/hp_common.h"

struct hp_bmap {
  unsigned     size;	/* In bits */
  HP_BMAP_UNIT *data;
};
typedef struct hp_bmap hp_bmap_var[1], *hp_bmap;

hp_bmap      hp_bmap_alloc(hp_bmap bm, unsigned size);
void         hp_bmap_free(hp_bmap bm);
hp_bmap      hp_bmap_clear_all(hp_bmap bm);
hp_bmap      hp_bmap_new(hp_bmap bm, unsigned size);
hp_bmap      hp_bmap_get(hp_bmap to, unsigned ofs, unsigned len, hp_bmap from);
uint32       hp_bmap_get32(hp_bmap from, unsigned ofs, unsigned len);
uint64       hp_bmap_get64(hp_bmap from, unsigned ofs, unsigned len);
hp_bmap      hp_bmap_set(hp_bmap to, unsigned ofs, unsigned len, hp_bmap from);
hp_bmap      hp_bmap_set32(hp_bmap to, unsigned ofs, unsigned len, uint32 val);
hp_bmap      hp_bmap_set64(hp_bmap to, unsigned ofs, unsigned len, uint64 val);
HP_BMAP_UNIT *hp_bmap_data(hp_bmap bm);
unsigned     hp_bmap_data_size(hp_bmap bm);











