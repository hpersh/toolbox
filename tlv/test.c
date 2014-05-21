#include "hp_tlv.h"

#include <stdio.h>
#include <assert.h>

unsigned char buf[1024];

void
dump(unsigned char *p, unsigned n)
{
  for ( ; n; --n, ++p)  printf("%c", *p);
}


void
tlv_dump(struct hp_tlv_node *nd)
{
  switch (nd->type) {
  case HP_TLV_TYPE_NIL:
    printf("#nil");
    break;
  case HP_TLV_TYPE_BOOL:
    printf(nd->val.boolval ? "#true" : "#false");
    break;
  case HP_TLV_TYPE_INT:
    printf("%d", nd->val.intval);
    break;
  case HP_TLV_TYPE_FLOAT:
    printf("%lg", nd->val.floatval);
    break;
  case HP_TLV_TYPE_STRING:
    printf("\"%s\"", nd->val.strval);
    break;
  case HP_TLV_TYPE_BYTES:
    {
      unsigned char *p;
      unsigned      n;

      for (p = nd->val.bytesval.data, n = nd->val.bytesval.len; n; --n, ++p)  printf("0x%02x", *p);
    }
    break;
  case HP_TLV_TYPE_WORDS:
    {
      unsigned short *p;
      unsigned       n;

      for (p = nd->val.wordsval.data, n = nd->val.wordsval.len; n; --n, ++p)  printf("0x%04x", *p);
    }
    break;
  case HP_TLV_TYPE_DWORDS:
    {
      unsigned long *p;
      unsigned      n;

      for (p = nd->val.dwordsval.data, n = nd->val.dwordsval.len; n; --n, ++p)  printf("0x%08lx", *p);
    }
    break;
  case HP_TLV_TYPE_QWORDS:
    {
      unsigned long long *p;
      unsigned           n;

      for (p = nd->val.qwordsval.data, n = nd->val.qwordsval.len; n; --n, ++p)  printf("0x%016llx", *p);
    }
    break;
  case HP_TLV_TYPE_LIST:
    {
      struct hp_list *p;
      char           c;
      
      for (c = '{', p = HP_LIST_FIRST(nd->val.listval); p != HP_LIST_END(nd->val.listval); p = HP_LIST_NEXT(p)) {
	putchar(c);
	tlv_dump(FIELD_PTR_TO_STRUCT_PTR(p, struct hp_tlv_node, siblings));
	c = ',';
      }

      printf("}");
    }
    break;
  default:
    assert(0);
  }
}

#if 0

struct field_desc {
  char             *fld_name;
  unsigned         fld_ofs, fld_len;
  enum hp_tlv_type elem_type;
  unsigned         elem_cnt;
  int              (*list_func)(void *);
};

struct struct_desc {
  unsigned          num_flds;
  struct field_desc *flds;
};

int
struct_tostring(struct hp_tlv_stream *st, void *s, struct struct_desc *d)
{
  hp_tlv_stream     fst, pst;
  unsigned          n;
  struct field_desc *fld;

  hp_tlv_tostring_list_begin(st, fst);

  for (fld = d->flds, n = d->num_flds; n; --n, ++fld) {
    hp_tlv_tostring_list_begin(fst, pst);
    hp_tlv_tostring_string(pst, fld->fld_name);
    switch (fld->elem_type) {
    case HP_TLV_TYPE_BOOL:
      break;
    case HP_TLV_TYPE_INT:
      {
	hp_tlv_intval val;
	unsigned      elem_size = fld->fld_len / fld->elem_cnt;
	unsigned char *p;

	for (p = (unsigned char *) s + fld->fld_ofs, n = fld->elem_cnt; n; --n, 
	

	switch (fld->


      hp_tlv_tostring_int(pr, t->i);
      hp_tlv_tostring_list_end(fields, pr);
    }
  }

  hp_tlv_tostring_list_end(st, fst);
}




struct test {
  int   i;
  float f;
  char  *s;
  int   a[10];
};

struct field_desc test_flds[] = {
  { "i", FIELD_OFS(struct test, i), FIELD_SIZE(struct test, i), HP_TLV_TYPE_INT, 1 },
  { "f", FIELD_OFS(struct test, f), FIELD_SIZE(struct test, f), HP_TLV_TYPE_FLOAT, 1 },
  { "s", FIELD_OFS(struct test, s), FIELD_SIZE(struct test, s), HP_TLV_TYPE_STRING, 1 },
  { "a", FIELD_OFS(struct test, a), FIELD_SIZE(struct test, a), HP_TLV_TYPE_INT, ARRAY_SIZE(((struct test *) 0)->a) }
};

struct struct_desc test_struct = {
  ARRAY_SIZE(test_flds), test_flds
};

#endif




struct test {
  int   i;
  float f;
  char  *s;
  int   a[10];
};

void
test_tlv_tostring(struct hp_tlv_stream *st, struct test *t)
{
  hp_tlv_stream fields, pr, arr;
  unsigned      i;

  hp_tlv_tostring_list_begin(st, fields);

  hp_tlv_tostring_list_begin(fields, pr);
  hp_tlv_tostring_string(pr, "i");
  hp_tlv_tostring_int(pr, t->i);
  hp_tlv_tostring_list_end(fields, pr);

  hp_tlv_tostring_list_begin(fields, pr);
  hp_tlv_tostring_string(pr, "f");
  hp_tlv_tostring_float(pr, t->f);
  hp_tlv_tostring_list_end(fields, pr);

  hp_tlv_tostring_list_begin(fields, pr);
  hp_tlv_tostring_string(pr, "s");
  hp_tlv_tostring_string(pr, t->s);
  hp_tlv_tostring_list_end(fields, pr);

  hp_tlv_tostring_list_begin(fields, pr);
  hp_tlv_tostring_string(pr, "a");
  hp_tlv_tostring_list_begin(pr, arr);
  for (i = 0; i < ARRAY_SIZE(t->a); ++i) {
    hp_tlv_tostring_int(arr, t->a[i]);
  }
  hp_tlv_tostring_list_end(pr, arr);
  hp_tlv_tostring_list_end(fields, pr);

  hp_tlv_tostring_list_end(st, fields);
}

int
test_tlv_parse_sax(struct hp_tlv_stream *st, struct test *t)
{
  enum hp_tlv_type type;
  unsigned         data_len, i;
  char             *data;
  hp_tlv_stream    fields, pr, arr;

  memset(t, 0, sizeof(*t));

  if (hp_tlv_parse_sax(st, &type, &data_len, &data) < 0
      || type != HP_TLV_TYPE_LIST
      )  return (-1);

  hp_tlv_stream_init(fields, data_len, data);

  while (!hp_tlv_stream_eof(fields)) {
    if (hp_tlv_parse_sax(fields, &type, &data_len, &data) < 0
	|| type != HP_TLV_TYPE_LIST
	)  return (-1);
    
    hp_tlv_stream_init(pr, data_len, data);

    if (hp_tlv_parse_sax(pr, &type, &data_len, &data) < 0
	|| type != HP_TLV_TYPE_STRING
	)  return (-1);
    
    if (data_len == 1 && strncmp(data, "i", 1) == 0) {
      hp_tlv_intval val;
      
      if (hp_tlv_parse_sax(pr, &type, &data_len, &data) < 0
	  || type != HP_TLV_TYPE_INT
	  || hp_tlv_parse_sax_int(data_len, data, &val) < 0
	  ) {
	return (-1);
      }
      
      t->i = val;
    } else if (data_len == 1 && strncmp(data, "f", 1) == 0) {
      hp_tlv_floatval val;
      
      if (hp_tlv_parse_sax(pr, &type, &data_len, &data) < 0
	  || type != HP_TLV_TYPE_FLOAT
	  || hp_tlv_parse_sax_float(data_len, data, &val) < 0
	  ) {
	return (-1);
      }
      
      t->f = val;
    } else if (data_len == 1 && strncmp(data, "s", 1) == 0) {
      unsigned bufsize;

      if (hp_tlv_parse_sax(pr, &type, &data_len, &data) < 0
	  || type != HP_TLV_TYPE_STRING
	  ) {
	return (-1);
      }
      
      bufsize = data_len + 1;
      t->s    = malloc(bufsize);
      if (t->s == 0)  return (-1);

      if (hp_tlv_parse_sax_string(data_len, data, bufsize, t->s) < 0)  return (-1);
    } else if (data_len == 1 && strncmp(data, "a", 1) == 0) {
      if (hp_tlv_parse_sax(pr, &type, &data_len, &data) < 0
	  || type != HP_TLV_TYPE_LIST
	  ) {
	return (-1);
      }

      hp_tlv_stream_init(arr, data_len, data);
      for (i = 0; i < 10; ++i) {
	hp_tlv_intval val;
      
	if (hp_tlv_stream_eof(arr)
	    || hp_tlv_parse_sax(arr, &type, &data_len, &data) < 0
	    || type != HP_TLV_TYPE_INT
	    || hp_tlv_parse_sax_int(data_len, data, &val) < 0
	    ) {
	  return (-1);
	}
	
	t->a[i] = val;
      }
      if (!hp_tlv_stream_eof(arr))  return (-1);
    } else  return (-1);

    if (!hp_tlv_stream_eof(pr))  return (-1);
  }
}

void
str_dump(unsigned len, char *s)
{
  for ( ; len; --len, ++s)  putchar(*s);
  putchar('\n');
}

struct test t1[1] = {
  42, 3.1415, "foo", { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23 }
}, t2[2];

char strbuf[1024];

int
main(void)
{
  hp_tlv_stream st;

  hp_tlv_stream_init(st, sizeof(strbuf), strbuf);

  test_tlv_tostring(st, t1);

  dump(strbuf, st->ofs);

  hp_tlv_stream_init(st, sizeof(strbuf), strbuf);

  test_tlv_parse_sax(st, t2);

  return (0);
}

#if 0
int
main(void)
{
  static const unsigned char bytes[] = { 0xde, 0xad, 0xbe, 0xef };

  struct hp_tlv_node *root, *pr, *pr2, *root2;
  hp_tlv_stream      st;
  int                len;

  root = hp_tlv_node_new(HP_TLV_TYPE_LIST);

  pr = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "version"), pr);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "1.1"), pr);
  hp_tlv_node_child_append(pr, root);
  
  pr = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "op"), pr);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "flow-add"), pr);
  hp_tlv_node_child_append(pr, root);
  
  pr = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "match"), pr);
  pr2 = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "ip-addr"), pr2);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "192.168.1.0"), pr2);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "255.255.255.0"), pr2);
  hp_tlv_node_child_append(pr2, pr);
  hp_tlv_node_child_append(pr, root);

  pr = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "actions"), pr);
  pr2 = hp_tlv_node_new(HP_TLV_TYPE_LIST);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_STRING, "output"), pr2);
  hp_tlv_node_child_append(hp_tlv_node_new(HP_TLV_TYPE_INT, 13), pr2);
  hp_tlv_node_child_append(pr2, pr);
  hp_tlv_node_child_append(pr, root);



  tlv_dump(root);
  printf("\n");

  hp_tlv_stream_init(st, sizeof(buf), buf);

  len = hp_tlv_tostring(sizeof(buf), buf, root);
  assert(len >= 0);
  dump(buf, (unsigned) len);
  printf("\n");

  hp_tlv_stream_init(st, len, buf);

  assert(hp_tlv_parse_dom(st, &root2) == 0);

  tlv_dump(root2);
  printf("\n");

  return (0);
}
#endif
