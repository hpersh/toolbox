


struct hp_tlv_db_node {
  struct hp_tlv_db_node *next;
  struct hp_tlv_node    *key, *data;
};

struct hp_tlv_db {
  int                   (*cmp)(struct hp_tlv_node *nd1, struct hp_tlv_node *nd2);
  struct hp_tlv_db_node *root;
};


int                   hp_tlv_db_insert(struct hp_tlv_db *db, struct hp_tlv_node *key, struct hp_tlv_node *data);
int                   hp_tlv_db_erase(struct hp_tlv_db *db, struct hp_tlv_node *key);
struct hp_tlv_db_node *hp_tlv_db_find(struct hp_tlv_db *db, struct hp_tlv_node *key);
struct hp_tlv_db_node *hp_tlv_db_first(struct hp_tlv_db *db);
struct hp_tlv_db_node *hp_tlv_db_last(struct hp_tlv_db *db);
struct hp_tlv_db_node *hp_tlv_db_prev(struct hp_tlv_db *db, struct hp_tlv_db_node *nd);
struct hp_tlv_db_node *hp_tlv_db_next(struct hp_tlv_db *db, struct hp_tlv_db_node *nd);
int                   hp_tlv_db_totring(struct hp_tlv_db *db, struct hp_tlv_stream *st);
int                   hp_tlv_db_parse(struct hp_tlv_db *db, struct hp_tlv_stream *st);


int
hp_tlv_db_insert(struct hp_tlv_db *db, struct hp_tlv_node *key, struct hp_tlv_node *data)
{
  struct hp_tlv_db_node **p, *q;

  for (p = &db->root; q = *p; p = &q->next) {
    if ((*db->cmp)(key, p->key) == 0)  return (-1);
  }

  nd = CMALLOC_T(struct
}

