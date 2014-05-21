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


#define TRY(x) \
  do { if ((n = (x)) < 0)  return (-1);  result += n; } while (0)

int
test_tlv_tostring(struct hp_tlv_stream *st, struct test *t)
{
  int result = 0, n;
  unsigned li_hofs, arr_hofs, i;

  TRY(hp_tlv_tostring_list_begin(st, TEST_TYPE_TEST, &li_hofs));

  TRY(hp_tlv_tostring_int(st, TEST_TYPE_I, t->i));
  TRY(n = hp_tlv_tostring_float(st, TEST_TYPE_F, t->f));
  TRY(hp_tlv_tostring_string(st, TEST_TYPE_S, t->s));
  TRY(hp_tlv_tostring_list_begin(st, TEST_TYPE_A, &arr_hofs));
  for (i = 0; i < ARRAY_SIZE(t->a); ++i) {
    TRY(hp_tlv_tostring_int(st, TEST_TYPE_A_ELEM, t->a[i]));
  }
  TRY(hp_tlv_tostring_list_end(st, TEST_TYPE_A, arr_hofs));

  TRY(hp_tlv_tostring_list_end(st, TEST_TYPE_TEST, li_hofs));

  return (result);
}

#define CHK_DECR(x, y) \
  do { if ((y) > (x))  return (-1);  (x) -= (y); } while (0)

int
test_tlv_parse_sax(struct hp_tlv_stream *st, struct test *t)
{
  int      result = 0, n;
  unsigned type, data_len, i, li_rem, pr_rem, arr_rem;

  memset(t, 0, sizeof(*t));

  TRY(hp_tlv_parse_sax_hdr(st, &type, &data_len));
  if (type != TEST_TYPE_TEST)  return (-1);

  for (li_rem = data_len; li_rem != 0; ) {
    TRY(hp_tlv_parse_sax_hdr(st, &type, &data_len));
    n += data_len;
    CHK_DECR(li_rem, n);

    switch (type) {
    case TEST_TYPE_I:
      {
	hp_tlv_intval val;
	
	TRY(hp_tlv_parse_sax_int(st, data_len, &val));
	
	t->i = val;
      }
      break;
    case TEST_TYPE_F:
      {
	hp_tlv_floatval val;
	
	TRY(hp_tlv_parse_sax_float(st, data_len, &val));
	
	t->f = val;
      }
      break;
    case TEST_TYPE_S:
      {
	unsigned bufsize = data_len + 1;

	t->s    = malloc(bufsize);
	if (t->s == 0)  return (-1);

	TRY(hp_tlv_parse_sax_string(st, data_len, t->s, bufsize));
      }
      break;
    case TEST_TYPE_A:
      {
	for (arr_rem = data_len, i = 0; i < 10; ++i) {
	  hp_tlv_intval val;
      
	  TRY(hp_tlv_parse_sax_hdr(st, &type, &data_len));
	  if (type != TEST_TYPE_A_ELEM)  return (-1);
	  n += data_len;
	  CHK_DECR(arr_rem, n);

	  TRY(hp_tlv_parse_sax_int(st, data_len, &val));
	
	  t->a[i] = val;
	}
      
	if (arr_rem != 0)  return (-1);
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

struct test_stream_buf {
  hp_tlv_stream base;
  char          *buf;
  unsigned      bufsize, ofs;
};

int
test_stream_buf_putc(struct hp_tlv_stream *st, char c)
{
  struct test_stream_buf *p = (struct test_stream_buf *) st;

  if (p->ofs >= p->bufsize)  return (-1);

  p->buf[p->ofs++] = c;

  return (0);
}

int
test_stream_buf_getc(struct hp_tlv_stream *st)
{
  struct test_stream_buf *p = (struct test_stream_buf *) st;

  if (p->ofs >= p->bufsize)  return (-1);

  return (p->buf[p->ofs++]);
}

int
test_stream_buf_tell(struct hp_tlv_stream *st, unsigned *ofs)
{
  struct test_stream_buf *p = (struct test_stream_buf *) st;

  *ofs = p->ofs;
  
  return (0);
}

int
test_stream_buf_seek(struct hp_tlv_stream *st, unsigned ofs)
{
  struct test_stream_buf *p = (struct test_stream_buf *) st;

  return (p->ofs = ofs);
}

void
test_stream_buf_init(struct test_stream_buf *ts, char *buf, unsigned bufsize)
{
  hp_tlv_stream_init(ts->base, test_stream_buf_putc, test_stream_buf_getc, test_stream_buf_tell, test_stream_buf_seek);
  ts->buf     = buf;
  ts->bufsize = bufsize;
  test_stream_buf_seek(ts->base, 0);
}

struct test_stream_file {
  hp_tlv_stream base;
  FILE          *fp;
};

int
test_stream_file_putc(struct hp_tlv_stream *st, char c)
{
  struct test_stream_file *p = (struct test_stream_file *) st;

  fputc(c, p->fp);

  return (1);
}

int
test_stream_file_getc(struct hp_tlv_stream *st)
{
  struct test_stream_file *p = (struct test_stream_file *) st;

  return (fgetc(p->fp));
}

int
test_stream_file_tell(struct hp_tlv_stream *st, unsigned *ofs)
{
  struct test_stream_file *p = (struct test_stream_file *) st;

  *ofs = ftell(p->fp);
  
  return (0);
}

int
test_stream_file_seek(struct hp_tlv_stream *st, unsigned ofs)
{
  struct test_stream_file *p = (struct test_stream_file *) st;

  return (fseek(p->fp, ofs, SEEK_SET));
}

void
test_stream_file_init(struct test_stream_file *ts, FILE *fp)
{
  hp_tlv_stream_init(ts->base, test_stream_file_putc, test_stream_file_getc, test_stream_file_tell, test_stream_file_seek);
  ts->fp = fp;
  test_stream_file_seek(ts->base, 0);
}


char test_buf[1024];

#define CHECK(x) \
  do { result = (x);  printf("%s = %d\n", #x, result);  assert(result >= 0); } while (0)

int
main(void)
{
  int                     result, oresult;
  struct test_stream_buf  tsb[1];
  struct test_stream_file tsf[1];
  FILE                    *fp;

  test_stream_buf_init(tsb, test_buf, sizeof(test_buf));

  CHECK(test_tlv_tostring(tsb->base, t1));
  assert(result == 116);

  dump(test_buf, tsb->ofs);
  printf("\n");

  test_stream_buf_init(tsb, test_buf, sizeof(test_buf));

  CHECK(test_tlv_parse_sax(tsb->base, t2));
  assert(result == 116);

  fp = fopen("/tmp/test.out", "w+");
  if (fp == 0)  perror("");

  test_stream_file_init(tsf, fp);

  CHECK(test_tlv_tostring(tsf->base, t1));
  assert(result == 116);

  test_stream_file_init(tsf, fp);

  CHECK(test_tlv_parse_sax(tsf->base, t2));
  assert(result == 116);

  return (0);
}
