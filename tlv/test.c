#include "shared/hp_common.h"
#include "hp_tlv.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

unsigned char buf[1024];

void
dump(unsigned char *p, unsigned n)
{
  for ( ; n; --n, ++p)  printf("%c", *p);
}


struct match {
  unsigned in_port, in_port_mask;
  unsigned dst_eth_addr, dst_eth_addr_mask;
  unsigned src_eth_addr, src_eth_addr_mask;
  unsigned dot1q_tag, dot1q_tag_mask;
  unsigned dot1q_tci, dot1q_tci_mask;
  unsigned ethertype, ethertype_mask;
  union {
    struct {
      unsigned src_ip_addr, src_ip_addr_mask;
      unsigned dst_ip_addr, dst_ip_addr_mask;
      unsigned ip_proto, ip_proto_mask;
      union {
	struct {
	  unsigned src_port, src_port_mask;
	  unsigned dst_port, dst_port_mask;
	} udp[1];
	struct {
	  unsigned flags, flags_mask;
	  unsigned src_port, src_port_mask;
	  unsigned dst_port, dst_port_mask;
	} tcp[1];
	struct {
	  unsigned type, type_mask;
	  unsigned code, code_mask;
	} icmp[1];
      } u[1];
    } ip[1];
  } u[1];
};

enum {
  TLV_TYPE_MATCH,
  TLV_TYPE_PORT,
  TLV_TYPE_PORT_MASK,
  TLV_TYPE_DST_ETH_ADDR,
  TLV_TYPE_DST_ETH_ADDR_MASK,
  TLV_TYPE_SRC_ETH_ADDR,
  TLV_TYPE_SRC_ETH_ADDR_MASK,
  TLV_TYPE_ACTION
};

enum action_op {
  ACTION_OP_OUTPUT,
  ACTION_OP_ADD_VLAN_TAG,
  ACTION_OP_STRIP_VLAN_TAG,
};

struct action {
  enum action_op op;
  union {
    struct {
      unsigned port;
    } output[1];
  } u[1];
};

struct flow {
  struct match  match[1];
  unsigned      num_actions;
  struct action *actions;
};





struct test {
  int      i;
  float    f;
  char     *s;
  int      a[10];
};

enum {
  TEST_TYPE_TEST = 0x10,
  TEST_TYPE_I,
  TEST_TYPE_F,
  TEST_TYPE_S,
  TEST_TYPE_A,
  TEST_TYPE_A_ELEM
};


#define TRYN(x) \
  do { if ((n = (x)) < 0)  return (-1);  result += n; } while (0)

int
test_tlv_tostring(struct hp_tlv_tostring_stream *st, struct test *t)
{
  int result = 0, n;
  struct hp_tlv_tostring_stream st_li[1], st_arr[1];
  unsigned i;

  TRYN(hp_tlv_tostring_list_begin(st, TEST_TYPE_TEST, st_li));

  TRYN(hp_tlv_tostring_int(st_li, TEST_TYPE_I, t->i));
  TRYN(hp_tlv_tostring_float(st_li, TEST_TYPE_F, t->f));
  TRYN(hp_tlv_tostring_string(st_li, TEST_TYPE_S, t->s));
  TRYN(hp_tlv_tostring_list_begin(st_li, TEST_TYPE_A, st_arr));
  for (i = 0; i < ARRAY_SIZE(t->a); ++i) {
    TRYN(hp_tlv_tostring_int(st_arr, TEST_TYPE_A_ELEM, t->a[i]));
  }
  TRYN(hp_tlv_tostring_list_end(st_arr));

  TRYN(hp_tlv_tostring_list_end(st_li));

  return (result);
}

int
test_tlv_parse(struct hp_tlv_parse_stream *st, struct test *t)
{
  int      result = 0, n;
  unsigned type, data_len, i;
  struct hp_tlv_parse_stream st_li[1], st_arr[1];

  memset(t, 0, sizeof(*t));

  TRYN(hp_tlv_parse_hdr(st, &type, &data_len));
  if (type != TEST_TYPE_TEST)  return (-1);

  for (hp_tlv_parse_list_begin(st, data_len, st_li);
       !hp_tlv_parse_list_end(st_li);
       ) {
    TRYN(hp_tlv_parse_hdr(st_li, &type, &data_len));

    switch (type) {
    case TEST_TYPE_I:
      {
	hp_tlv_intval val;
	
	TRYN(hp_tlv_parse_int(st_li, data_len, &val));
	
	t->i = val;
      }
      break;
    case TEST_TYPE_F:
      {
	hp_tlv_floatval val;
	
	TRYN(hp_tlv_parse_float(st_li, data_len, &val));
	
	t->f = val;
      }
      break;
    case TEST_TYPE_S:
      {
	unsigned bufsize = data_len + 1;

	t->s    = malloc(bufsize);
	if (t->s == 0)  return (-1);

	TRYN(hp_tlv_parse_string(st_li, data_len, t->s, bufsize));
      }
      break;
    case TEST_TYPE_A:
      {
	for (i = 0, hp_tlv_parse_list_begin(st_li, data_len, st_arr);
	     !hp_tlv_parse_list_end(st_arr);
	     ++i) {
	  hp_tlv_intval val;
      
	  if (i >= ARRAY_SIZE(t->a))  return (-1);

	  TRYN(hp_tlv_parse_hdr(st_arr, &type, &data_len));
	  if (type != TEST_TYPE_A_ELEM)  return (-1);
	  TRYN(hp_tlv_parse_int(st_arr, data_len, &val));
	
	  t->a[i] = val;
	}

	if (i != ARRAY_SIZE(t->a))  return (-1);
      }
      break;
    default:
      return (-1);
    }
  }

  return (result);
}

void
str_dump(unsigned len, char *s)
{
  for ( ; len; --len, ++s)  putchar(*s);
  putchar('\n');
}

struct test t1[1] = {
  42, 3.1415, "foo", { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23 }
}, t2[1];



char test_buf[1024];
FILE *fp;

#define CHECK(x) \
  do { result = (x);  printf("%s = %d\n", #x, result);  assert(result >= 0); } while (0)

int
main(void)
{
  int                           result;
  struct hp_stream_file         iost[1];
  struct hp_tlv_tostring_stream st_out[1];
  struct hp_tlv_parse_stream    st_in[1];
  unsigned              i;

  fp = fopen("test.tlv", "w+");
  assert(fp != 0);

  hp_stream_file_init(iost, fp);
  hp_tlv_tostring_stream_init(st_out, iost->base);

  CHECK(test_tlv_tostring(st_out, t1));
  assert(result == 116);

  fclose(fp);

  fp = fopen("test.tlv", "r");
  assert(fp != 0);

  hp_stream_file_init(iost, fp);
  hp_tlv_parse_stream_init(st_in, iost->base);

  CHECK(test_tlv_parse(st_in, t2));
  assert(result == 116);
  assert(t2->i == t1->i);
  assert(t2->f == t1->f);
  assert(strcmp(t2->s, t2->s) == 0);
  for (i = 0; i < ARRAY_SIZE(t2->a); ++i) {
    assert(t2->a[i] == t2->a[i]);
  }

  return (0);
}
