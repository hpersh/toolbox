#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FIELD_OFS(s, f)                   ((int)&((s *) 0)->f)
#define FIELD_PTR_TO_STRUCT_PTR(p, s, f)  ((s *)((char *)(p) - FIELD_OFS(s, f)))

struct list {
  struct list *prev, *next;
};

#define LIST_INIT(li)   ((li)->prev = (li)->next = (li))
#define LIST_FIRST(li)  ((li)->next)
#define LIST_LAST(li)   ((li)->prev)
#define LIST_END(li)    (li)
#define LIST_EMPTY(li)  (LIST_FIRST(li) == LIST_END(li))
#define LIST_PREV(nd)   ((nd)->prev)
#define LIST_NEXT(nd)   ((nd)->next)

struct list *
list_insert(struct list *nd, struct list *before)
{
  struct list *p = before->prev;

  nd->prev = p;
  nd->next = before;

  return (p->next = before->prev = nd);  
}


struct tlv_node {
  struct list siblings[1], children[1];

  unsigned      type, data_len;
  unsigned char *data;
};


struct tlv_node *
tlv_node_insert(unsigned type, unsigned data_len, unsigned char *data, struct list *before)
{
  struct tlv_node *result = (struct tlv_node *) calloc(1, sizeof(struct tlv_node));

  LIST_INIT(result->siblings);
  LIST_INIT(result->children);
  result->type     = type;
  result->data_len = data_len;
  result->data     = data;

  if (before)  list_insert(result->siblings, before);

  return (result);
}


struct tlv_node *
tlv_find(struct tlv_node *root, char *tlv_path)
{
  char *ss;

  for (s = p = tlv_path; ; s = 0) {
    q = strtok_r(s, "/", &ss);

    if (q == 0)  break;

    p = q + 1;
  }
}



struct tlv_stream {
  unsigned char *buf;
  unsigned      size;
  unsigned      ofs;
};

void
tlv_stream_init(struct tlv_stream *st, unsigned char *buf, unsigned len)
{
  st->buf  = buf;
  st->size = len;
  st->ofs  = 0;
}

unsigned 
tlv_stream_ofs(struct tlv_stream *st)
{
  return (st->ofs);
}

unsigned
tlv_stream_rem(struct tlv_stream *st)
{
  return (st->size - st->ofs);
}

int
tlv_stream_chk(struct tlv_stream *st, unsigned len)
{
  return ((st->ofs + len) <= st->size ? 0 : -1);
}

void
tlv_stream_adv(struct tlv_stream *st, unsigned len)
{
  st->ofs += len;
}

unsigned char *
tlv_stream_ptr(struct tlv_stream *st)
{
  return (st->buf + st->ofs);
}

int
tlv_hdr_put(struct tlv_stream *st, unsigned type, unsigned data_len)
{
  unsigned char *p;

  assert(type <= 0xff);
  assert(data_len <= 0xff);

  if (tlv_stream_chk(st, 2 + data_len) < 0)  return (-1);

  p = tlv_stream_ptr(st);
  p[0] = type;
  p[1] = data_len;
  tlv_stream_adv(st, 2);

  return (0);
}

int
tlv_hdr_get(struct tlv_stream *st, unsigned *type, unsigned *data_len)
{
  unsigned char *p;

  if (tlv_stream_chk(st, 2) < 0)  return (-1);
  
  p = tlv_stream_ptr(st);
  *type     = p[0];
  *data_len = p[1];

  if (tlv_stream_chk(st, 2 + *data_len) < 0)  return (-1);

  tlv_stream_adv(st, 2);
  
  return (0);
}


int
tlv_collect(struct tlv_stream *st, struct tlv_node *nd)
{
  struct tlv_stream sub[1];
  unsigned          data_len;

  tlv_stream_init(sub, tlv_stream_ptr(st), tlv_stream_rem(st));

  if (tlv_hdr_put(sub, nd->type, 0) < 0)  return (-1);

  if (LIST_EMPTY(nd->children)) {
    data_len = nd->data_len;
    if (tlv_stream_chk(sub, data_len) < 0)  return (-1);
    memcpy(tlv_stream_ptr(sub), nd->data, data_len);
  } else {
    unsigned    data_start = tlv_stream_ofs(sub);
    struct list *p;

    for (p = LIST_FIRST(nd->children); p != LIST_END(nd->children); p = LIST_NEXT(p)) {
      if (tlv_collect(sub, FIELD_PTR_TO_STRUCT_PTR(p, struct tlv_node, siblings)) < 0)  return (-1);
    }
    
    data_len = tlv_stream_ofs(sub) - data_start;
  }
  
  if (tlv_hdr_put(st, nd->type, data_len) < 0)  return (-1);
  tlv_stream_adv(st, data_len);

  return (0);
}


int
_tlv_parse(struct tlv_stream *st, unsigned (*tlv_type_is_composite)(unsigned), struct tlv_node *parent, struct tlv_node **nd)
{
  unsigned        type, data_len;
  struct tlv_node *p;

  if (tlv_hdr_get(st, &type, &data_len) < 0)  return (-1);

  *nd = p = tlv_node_insert(type, data_len, tlv_stream_ptr(st), parent ? LIST_END(parent->children) : 0);

  if ((*tlv_type_is_composite)(type)) {
    struct tlv_stream sub[1];
    struct tlv_node   *child;

    for (tlv_stream_init(sub, tlv_stream_ptr(st), data_len); tlv_stream_rem(sub) != 0; ) {
      if (_tlv_parse(sub, tlv_type_is_composite, p, &child) < 0)  return (-1);
    }
  }

  tlv_stream_adv(st, data_len);

  return (0);
}

int
tlv_parse(struct tlv_stream *st, unsigned (*tlv_type_is_composite)(unsigned), struct tlv_node **root)
{
  return (_tlv_parse(st, tlv_type_is_composite, 0, root));
}


#ifdef __UNIT_TEST__

#include <stdio.h>

unsigned char buf[1024];

struct tlv_node *root;

unsigned
tlv_type_is_composite(unsigned type)
{
  return ((type >> 7) & 1);
}

char data1[] = "Sam I am", data2[] = "Now is the winter";

void
hex_dump(unsigned char *p, unsigned n)
{
  for ( ; n; --n, ++p)  printf("%02x", *p);
}

void
_tlv_dump(struct tlv_node *nd, unsigned lvl)
{
  for ( ; lvl; --lvl)  printf("  ");

  printf("%02x%02x", nd->type, nd->data_len);

  if (LIST_EMPTY(nd->children)) {
    hex_dump(nd->data, nd->data_len);
    printf("\n");
  } else {
    struct list *p;

    printf("\n");
  
    for (p = LIST_FIRST(nd->children); p != LIST_END(nd->children); p = LIST_NEXT(p)) {
      _tlv_dump(FIELD_PTR_TO_STRUCT_PTR(p, struct tlv_node, siblings), lvl + 1);
    }
  }
}

void
tlv_dump(struct tlv_node *nd)
{
  _tlv_dump(nd, 0);
}


int
main(void)
{
  struct tlv_node   *nd1, *nd2, *nd3, *nd4;
  struct tlv_stream st[1];

  nd1 = tlv_node_insert(0x80, 0, 0, 0);
  nd2 = tlv_node_insert(0x1, strlen(data1), data1, LIST_END(nd1->children));
  nd3 = tlv_node_insert(0x42, strlen(data2), data2, LIST_END(nd1->children));

  tlv_stream_init(st, buf, sizeof(buf));

  assert(tlv_collect(st, nd1) == 0);

  hex_dump(buf, tlv_stream_ofs(st));
  printf("\n");

  tlv_stream_init(st, buf, tlv_stream_ofs(st));

  assert(tlv_parse(st, tlv_type_is_composite, &nd4) == 0);

  tlv_dump(nd4);

  return (0);
}

#endif /* defined(__UNIT_TEST__) */
