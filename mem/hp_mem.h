
#include "common/hp_common.h"

typedef void *hp_mem;

hp_mem   hp_mem_alloc(unsigned size);
hp_mem   hp_mem_zalloc(unsigned size);
hp_mem   hp_mem_validate(hp_mem mem);
unsigned hp_mem_size(hp_mem mem);
uint8    *hp_mem_data(hp_mem mem, unsigned ofs, unsigned len);
void     hp_free(hp_mem mem);
