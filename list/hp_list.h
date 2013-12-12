
struct hp_list {
  struct hp_list *prev, *next;
};
typedef struct hp_list hp_list[1];

#define HP_LIST_INIT(li)   ((li)->prev = (li)->next = (li))
#define HP_LIST_FIRST(li)  ((li)->next)
#define HP_LIST_LAST(li)   ((li)->prev)
#define HP_LIST_END(li)    (li)
#define HP_LIST_EMPTY(li)  (HP_LIST_FIRST(li) == HP_LIST_END(li))
#define HP_LIST_PREV(nd)   ((nd)->prev)
#define HP_LIST_NEXT(nd)   ((nd)->next)

struct hp_list *hp_list_insert(struct hp_list *nd, struct hp_list *before);
struct hp_list *hp_list_erase(struct hp_list *nd);
