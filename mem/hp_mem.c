#include "hp_mem.h"
#include "hp_mem_cfg.h"

#include "common/hp_common.h"
#include "obj/hp_obj.h"
#include "except/hp_except.h"

#include <stdlib.h>
#include <string.h>


struct hdr {
  struct hp_obj base[1];
  unsigned      size;
};

enum {
  HP_OBJ_TYPE_MEM = 0x48504d45	/* "HPME" */
};

#ifdef HP_MEM_STATS
struct {
  unsigned alloced_cnt;
  unsigned alloced_bytes;
  unsigned freed_cnt;
  unsigned freed_bytes;
} hp_mem_stats[1];
#endif

hp_mem
hp_mem_alloc(unsigned size)
{
  struct hdr *p;
  void       *result = malloc(sizeof(*p) + size);

  if (result == 0)  HP_EXCEPT_RAISE(HP_EXCEPT_NO_MEM, 0);

  p = (struct hdr *) result;
  p->base->type = HP_OBJ_TYPE_MEM;
  p->size        = size;

#ifdef HP_MEM_STATS
  ++hp_mem_stats->alloced_cnt;
  hp_mem_stats->alloced_bytes += size;
#endif

  return (result);
}

hp_mem
hp_mem_zalloc(unsigned size)
{
  hp_mem result;

  if (result = hp_mem_alloc(size)) {
    memset(hp_mem_data(result, 0, size), 0, size);
  }

  return (result);
}

hp_mem
hp_mem_validate(hp_mem mem)
{
  if (!hp_obj_validate((struct hdr *) mem)->base, HP_OBJ_TYPE_MEM)  HP_EXCEPT_RAISE(HP_EXCEPT_BAD_REF, mem);
  
  return (mem);
}

unsigned
hp_mem_size(hp_mem mem)
{
  hp_mem_validate(mem);

  return (((struct hdr *) mem)->size);
}

uint8 *
hp_mem_data(hp_mem mem, unsigned ofs, unsigned len)
{
  struct hdr *q;

  hp_mem_validate(mem);

  q = (struct hdr *) mem;
  if ((ofs + len) > q->size)  HP_EXCEPT_RAISE(HP_EXCEPT_IDX_RANGE, mem);

  return ((uint8 *)(q + 1) + ofs);
}

void
hp_mem_free(hp_mem mem)
{
  struct hdr *q;

  if (mem == 0)  return;

  hp_mem_validate(mem);
  
  q = (struct hdr *) mem;

#ifdef HP_MEM_STATS
  ++hp_mem_stats->freed_cnt;
  hp_mem_stats->freed_bytes += q->size;
#endif

  q->base->type = 0;
  q->size       = 0;

  free(mem);
}


#ifdef __UNIT_TEST__

int
main(void)
{
  

  return (0);
}

#endif
