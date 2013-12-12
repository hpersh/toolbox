#include "list/hp_list.h"

struct hp_ht_node {
  struct hp_list list_node;
  void           *key, *data;
};
typedef struct hp_ht_node hp_ht_node[1];

struct hp_ht {
  unsigned       size;
  struct hp_list *data;
  unsigned       cnt;
  unsigned       (*key_hash)(void *);
  int            (*key_cmp)(void *, void *);
};
typedef struct hp_ht hp_ht[1];
