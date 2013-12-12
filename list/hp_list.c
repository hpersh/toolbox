#include "hp_list.h"

struct hp_list *
hp_list_insert(struct hp_list * const nd, struct hp_list * const before)
{
  struct hp_list *p = before->prev;

  nd->prev = p;
  nd->next = before;

  return (p->next = before->prev = nd);
}

struct hp_list *
hp_list_erase(struct hp_list * const nd)
{
  struct hp_list *p = nd->prev, *q = nd->next;

  (p->next = q)->prev = p;

  nd->prev = nd->next = 0;

  return (nd);
}


#ifdef __UNIT_TEST__

#include <assert.h>

int
main(void)
{
  hp_list li, nd[3];

  HP_LIST_INIT(li);

  assert(HP_LIST_EMPTY(li));
  assert(HP_LIST_FIRST(li) == HP_LIST_END(li));
  assert(HP_LIST_LAST(li) == HP_LIST_END(li));

  hp_list_insert(nd[0], HP_LIST_END(li));

  assert(!HP_LIST_EMPTY(li));
  assert(HP_LIST_FIRST(li) == nd[0]);
  assert(HP_LIST_LAST(li) == nd[0]);
  assert(HP_LIST_PREV(nd[0]) == HP_LIST_END(li));
  assert(HP_LIST_NEXT(nd[0]) == HP_LIST_END(li));
  
  return (0);
}

#endif
