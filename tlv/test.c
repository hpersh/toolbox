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

