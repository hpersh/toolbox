#include "hp_ht.h"
#include "mem/hp_mem.h"
#include "common/hp_common.h"

struct hp_ht *
hp_ht_init(struct hp_ht * const ht, unsigned size, unsigned (* const key_hash)(void *), int (* const key_cmp)(void *, void *))
{
  struct hp_list *p;
  unsigned       n;

  ht->size = size;
  ht->data = (struct hp_list *) hp_malloc(size * sizeof(struct hp_list));
  for (p = ht->data, n = size; n; --n, ++p)  HP_LIST_INIT(p);
  ht->cnt      = 0;
  ht->key_hash = key_hash;
  ht->key_cmp  = key_cmp;

  return (ht);
}

void
hp_ht_free(struct hp_ht * const ht)
{
  if (ht->data != 0)  hp_free(ht->data);

  memset(ht, 0, sizeof(*ht));
}

static struct hp_ht_node *
ht_find(struct hp_ht * const ht, void *key, struct hp_list **bucket)
{
  struct hp_list *b = &ht->data[(*ht->key_hash)(key) % ht->size], *p;

  if (bucket)  *bucket = b;

  for (p = HP_LIST_FIRST(b); p != HP_LIST_END(b); p = HP_LIST_NEXT(p)) {
    struct hp_ht_node *q = FIELD_PTR_TO_STRUCT_PTR(p, struct hp_ht_node, list_node);

    if ((*ht->key_cmp)(q->key, key) == 0)  return (q);
  }
  
  return (0);
}

struct hp_ht_node *
hp_ht_find(struct hp_ht * const ht, void *key)
{
  return (ht_find(ht, key, 0));
}

struct hp_ht_node *
hp_ht_insert(struct hp_ht * const ht, struct hp_ht_node * const htn, void *key, void *data)
{
  struct hp_list *bucket;

  if (ht_find(ht, key, &bucket))  return (0);

  htn->key  = key;
  htn->data = data;

  hp_list_insert(htn->list_node, HP_LIST_END(bucket));

  ++ht->cnt;

  return (htn);
}

struct hp_ht_node *
hp_ht_erase(struct hp_ht_node * const htn)
{
  hp_list_erase(htn->list_node);

  return (htn);
}

void *
hp_ht_key(struct hp_ht_node * const htn)
{
  return (htn->key);
}

void *
hp_ht_data(struct hp_ht_node * const htn)
{
  return (htn->data);
}


#ifdef __UNIT_TEST__

#include <string.h>

unsigned
test_hash(void *p)
{
  unsigned result;
  char     *s, c;

  for (result = 0, s = (char *) p; c = *s; ++s)  result = 37 * result + c;

  return (result);
}


int
main(void)
{
  hp_ht      ht1;
  hp_ht_node htn[3];

  hp_ht_init(ht1, 16, test_hash, strcmp);

  return (0);
}

#endif
