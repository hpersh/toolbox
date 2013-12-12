
struct ht_node {
  struct list list_node[1];
  void        *key, *data;
};

struct ht {
  unsigned    size, cnt;
  struct list *data;
  unsigned    (*key_hash)(void *);
  int         (*key_cmp)(void *, void *);
};

typedef struct ht_node ht_node[1];
typedef struct ht      ht[1];

struct ht      *ht_init(struct ht * const ht, unsigned size, unsigned (*key_hash)(void *), int (*key_cmp)(void *, void *));
void           ht_free(struct ht * const ht);
struct ht_node *ht_insert(struct ht * const ht, struct ht_node * const htn, void *key, void *data);
struct ht_node *ht_erase(struct ht_node * const htn);
struct ht_node *ht_find(struct ht * const ht, void *key);
void           *ht_key(struct ht_node * const htn);
void           *ht_data(struct ht_node * const htn);
