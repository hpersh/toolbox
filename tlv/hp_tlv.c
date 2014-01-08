#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "common/hp_common.h"
#include "list/hp_list.h"
#include "hp_tlv.h"


#define BITS_TO_CHARS(x)  (((x) + 3) / 4)

enum {
  HP_TLV_HDR_BITS  = HP_TLV_HDR_TYPE_BITS + HP_TLV_HDR_LEN_BITS, /* TLV header length, in bits */
  HP_TLV_HDR_CHARS = BITS_TO_CHARS(HP_TLV_HDR_BITS)
};


static struct hp_tlv_node *
tlv_node_new(enum hp_tlv_type type)
{
  struct hp_tlv_node *result;

  if (result = CALLOC_T(struct hp_tlv_node, 1)) {
    HP_LIST_INIT(result->siblings);
    result->type = type;
  }

  return (result);
}


struct hp_tlv_node *
hp_tlv_node_new(enum hp_tlv_type type, ...)
{
  struct hp_tlv_node *result;

  if (result = tlv_node_new(type)) {
    va_list            ap;

    va_start(ap, type);
    
    switch (type) {
    case HP_TLV_TYPE_NIL:
      break;
    case HP_TLV_TYPE_BOOL:
      result->val.boolval = (va_arg(ap, unsigned) != 0);
      break;
    case HP_TLV_TYPE_INT:
      result->val.intval = va_arg(ap, hp_tlv_intval);
      break;
    case HP_TLV_TYPE_FLOAT:
      result->val.floatval = va_arg(ap, hp_tlv_floatval);
      break;
    case HP_TLV_TYPE_STRING:
      result->val.strval = strdup(va_arg(ap, char *));
      break;
    case HP_TLV_TYPE_BYTES:
      result->val.bytesval.len = va_arg(ap, unsigned);
      if ((result->val.bytesval.data = malloc(result->val.bytesval.len)) == 0) {
	free(result);
	result = 0;
      }

      memcpy(result->val.bytesval.data, va_arg(ap, unsigned char *), result->val.bytesval.len);
      break;
    case HP_TLV_TYPE_WORDS:
      {
	unsigned n;

	result->val.wordsval.len = va_arg(ap, unsigned);
	n = result->val.wordsval.len << 1;
	if ((result->val.wordsval.data = malloc(n)) == 0) {
	  free(result);
	  result = 0;
	}
	
	memcpy(result->val.wordsval.data, va_arg(ap, unsigned char *), n);
      }
      break;
    case HP_TLV_TYPE_DWORDS:
      {
	unsigned n;

	result->val.dwordsval.len = va_arg(ap, unsigned);
	n = result->val.dwordsval.len << 2;
	if ((result->val.dwordsval.data = malloc(n)) == 0) {
	  free(result);
	  result = 0;
	}
	
	memcpy(result->val.dwordsval.data, va_arg(ap, unsigned char *), n);
      }
      break;
    case HP_TLV_TYPE_QWORDS:
      {
	unsigned n;

	result->val.qwordsval.len = va_arg(ap, unsigned);
	n = result->val.qwordsval.len << 3;
	if ((result->val.qwordsval.data = malloc(n)) == 0) {
	  free(result);
	  result = 0;
	}
	
	memcpy(result->val.qwordsval.data, va_arg(ap, unsigned char *), n);
      }
      break;
    case HP_TLV_TYPE_LIST:
      HP_LIST_INIT(result->val.listval);
      break;
    default:
      assert(0);
    }
    
    va_end(ap);
  }

  return (result);
}


void
hp_tlv_node_delete(struct hp_tlv_node *nd)
{
  switch (nd->type) {
  case HP_TLV_TYPE_NIL:
  case HP_TLV_TYPE_BOOL:
  case HP_TLV_TYPE_INT:
  case HP_TLV_TYPE_FLOAT:
    break;
  case HP_TLV_TYPE_STRING:
    free(nd->val.strval);
    break;
  case HP_TLV_TYPE_BYTES:
    free(nd->val.bytesval.data);
    break;
  case HP_TLV_TYPE_WORDS:
    free(nd->val.wordsval.data);
    break;
  case HP_TLV_TYPE_DWORDS:
    free(nd->val.dwordsval.data);
    break;
  case HP_TLV_TYPE_QWORDS:
    free(nd->val.qwordsval.data);
    break;
  case HP_TLV_TYPE_LIST:
    {
      struct hp_list *p;

      while ((p = HP_LIST_FIRST(nd->val.listval)) != HP_LIST_END(nd->val.listval)) {
	hp_list_erase(p);
	hp_tlv_node_delete(FIELD_PTR_TO_STRUCT_PTR(p, struct hp_tlv_node, siblings));
      }
    }
    break;
  default:
    assert(0);
  }

  free(nd);
}


struct hp_tlv_node *
hp_tlv_node_child_insert(struct hp_tlv_node *nd, struct hp_tlv_node *parent, struct hp_list *before)
{
  assert(parent->type == HP_TLV_TYPE_LIST);

  hp_list_insert(nd->siblings, before);
  nd->parent = parent;

  return (nd);
}


struct hp_tlv_node *
hp_tlv_node_child_append(struct hp_tlv_node *nd, struct hp_tlv_node *parent)
{
  return (hp_tlv_node_child_insert(nd, parent, HP_LIST_END(parent->val.listval)));
}


struct hp_tlv_node *
hp_tlv_node_erase(struct hp_tlv_node *nd)
{
  hp_list_erase(nd->siblings);

  nd->parent = 0;
  HP_LIST_INIT(nd->siblings);
}


void
hp_tlv_stream_init(struct hp_tlv_stream *st, unsigned len, unsigned char *buf)
{
  st->buf  = buf;
  st->size = len;
  st->ofs  = 0;
}

static unsigned 
tlv_stream_ofs(struct hp_tlv_stream *st)
{
  return (st->ofs);
}

static unsigned
tlv_stream_rem(struct hp_tlv_stream *st)
{
  return (st->size - st->ofs);
}

static int
tlv_stream_chk(struct hp_tlv_stream *st, unsigned len)
{
  return ((st->ofs + len) <= st->size ? 0 : -1);
}

static void
tlv_stream_adv(struct hp_tlv_stream *st, unsigned len)
{
  st->ofs += len;
}

static unsigned char *
tlv_stream_ptr(struct hp_tlv_stream *st)
{
  return (st->buf + st->ofs);
}

static void
tlv_stream_uint_put(struct hp_tlv_stream *st, unsigned n, unsigned val)
{
  char buf[9];

  sprintf(buf, "%08x", val);
  memcpy(tlv_stream_ptr(st), buf + 8 - n, n);
}

static int
tlv_stream_uint_get(struct hp_tlv_stream *st, unsigned n, unsigned *val)
{
  char buf[9];

  memcpy(buf, tlv_stream_ptr(st), n);
  buf[n] = 0;

  return (sscanf(buf, "%x", val) == 1 ? 0 : -1);
}

static int
tlv_hdr_put(struct hp_tlv_stream *st, unsigned type, unsigned data_len)
{
  if (type >= (1 << HP_TLV_HDR_TYPE_BITS)
      || data_len >= (1 << HP_TLV_HDR_LEN_BITS)
      || tlv_stream_chk(st, HP_TLV_HDR_CHARS + data_len) < 0
      )  return (-1);

  tlv_stream_uint_put(st, HP_TLV_HDR_CHARS, (type << HP_TLV_HDR_LEN_BITS) | data_len);
  tlv_stream_adv(st, HP_TLV_HDR_CHARS);

  return (0);
}

static int
tlv_hdr_get(struct hp_tlv_stream *st, unsigned *type, unsigned *data_len)
{
  unsigned h;

  if (tlv_stream_chk(st, HP_TLV_HDR_CHARS) < 0)  return (-1);
  tlv_stream_uint_get(st, HP_TLV_HDR_CHARS, &h);
  *type     = h >> HP_TLV_HDR_LEN_BITS;
  *data_len = h & ((1 << HP_TLV_HDR_LEN_BITS) - 1);

  tlv_stream_adv(st, HP_TLV_HDR_CHARS);

  return (tlv_stream_chk(st, *data_len));
}


int
hp_tlv_tostring_nil(struct hp_tlv_stream *st)
{
  return (tlv_hdr_put(st, HP_TLV_TYPE_NIL, 0));
}


int
hp_tlv_tostring_bool(struct hp_tlv_stream *st, unsigned val)
{
  if (tlv_hdr_put(st, HP_TLV_TYPE_BOOL, 1) < 0)  return (-1);
  *tlv_stream_ptr(st) = val ? 'T' : 'F';
  tlv_stream_adv(st, 1);

  return (0);  
}


int
hp_tlv_tostring_int(struct hp_tlv_stream *st, int val)
{
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_INT, val);
  data_len = strlen(data_buf);

  if (tlv_hdr_put(st, HP_TLV_TYPE_INT, data_len) < 0)  return (-1);
  memcpy(tlv_stream_ptr(st), data_buf, data_len);
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_float(struct hp_tlv_stream *st, double val)
{
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_FLOAT, val);
  data_len = strlen(data_buf);

  if (tlv_hdr_put(st, HP_TLV_TYPE_FLOAT, data_len) < 0)  return (-1);
  memcpy(tlv_stream_ptr(st), data_buf, data_len);
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_string(struct hp_tlv_stream *st, char *s)
{
  unsigned data_len = strlen(s);

  if (tlv_hdr_put(st, HP_TLV_TYPE_STRING, data_len) < 0)  return (-1);
  memcpy(tlv_stream_ptr(st), s, data_len);
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_bytes(struct hp_tlv_stream *st, unsigned bytes_len, unsigned char *bytes)
{
  unsigned data_len = 2 * bytes_len;
  char     data_buf[2 + 1], *p;

  if (tlv_hdr_put(st, HP_TLV_TYPE_BYTES, data_len) < 0)  return (-1);
  for (p = tlv_stream_ptr(st); bytes_len; --bytes_len, ++bytes, p += 2) {
    sprintf(data_buf, "%02x", *bytes);
    memcpy(p, data_buf, 2);
  }
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_words(struct hp_tlv_stream *st, unsigned words_len, unsigned short *words)
{
  unsigned data_len = 4 * words_len;
  char     data_buf[4 + 1], *p;

  if (tlv_hdr_put(st, HP_TLV_TYPE_WORDS, data_len) < 0)  return (-1);
  for (p = tlv_stream_ptr(st); words_len; --words_len, ++words, p += 4) {
    sprintf(data_buf, "%04x", *words);
    memcpy(p, data_buf, 4);
  }
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_dwords(struct hp_tlv_stream *st, unsigned dwords_len, unsigned long *dwords)
{
  unsigned data_len = 8 * dwords_len;
  char     data_buf[8 + 1], *p;

  if (tlv_hdr_put(st, HP_TLV_TYPE_DWORDS, data_len) < 0)  return (-1);
  for (p = tlv_stream_ptr(st); dwords_len; --dwords_len, ++dwords, p += 8) {
    sprintf(data_buf, "%08lx", *dwords);
    memcpy(p, data_buf, 8);
  }
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_qwords(struct hp_tlv_stream *st, unsigned qwords_len, unsigned long long *qwords)
{
  unsigned data_len = 16 * qwords_len;
  char     data_buf[16 + 1], *p;

  if (tlv_hdr_put(st, HP_TLV_TYPE_QWORDS, data_len) < 0)  return (-1);
  for (p = tlv_stream_ptr(st); qwords_len; --qwords_len, ++qwords, p += 16) {
    sprintf(data_buf, "%016llx", *qwords);
    memcpy(p, data_buf, 16);
  }
  tlv_stream_adv(st, data_len);

  return (0);  
}


int
hp_tlv_tostring_list_begin(struct hp_tlv_stream *st, struct hp_tlv_stream *sub)
{
  if (tlv_stream_chk(st, HP_TLV_HDR_CHARS) < 0)  return (-1);

  hp_tlv_stream_init(sub, tlv_stream_rem(st) - HP_TLV_HDR_CHARS, tlv_stream_ptr(st) + HP_TLV_HDR_CHARS);

  return (0);
}


int
hp_tlv_tostring_list_end(struct hp_tlv_stream *st, struct hp_tlv_stream *sub)
{
  unsigned data_len = tlv_stream_ofs(sub);

  if (tlv_hdr_put(st, HP_TLV_TYPE_LIST, data_len) < 0)  return (-1);

  tlv_stream_adv(st, data_len);

  return (0);
}


int
hp_tlv_tostring(struct hp_tlv_stream *st, struct hp_tlv_node *nd)
{
  switch (nd->type) {
  case HP_TLV_TYPE_NIL:
    return (hp_tlv_tostring_nil(st));
  case HP_TLV_TYPE_BOOL:
    return (hp_tlv_tostring_bool(st, nd->val.boolval));
  case HP_TLV_TYPE_INT:
    return (hp_tlv_tostring_int(st, nd->val.intval));
  case HP_TLV_TYPE_FLOAT:
    return (hp_tlv_tostring_float(st, nd->val.floatval));
  case HP_TLV_TYPE_STRING:
    return (hp_tlv_tostring_string(st, nd->val.strval));
  case HP_TLV_TYPE_BYTES:
    return (hp_tlv_tostring_bytes(st, nd->val.bytesval.len, nd->val.bytesval.data));
  case HP_TLV_TYPE_WORDS:
    return (hp_tlv_tostring_words(st, nd->val.wordsval.len, nd->val.wordsval.data));
  case HP_TLV_TYPE_DWORDS:
    return (hp_tlv_tostring_dwords(st, nd->val.dwordsval.len, nd->val.dwordsval.data));
  case HP_TLV_TYPE_QWORDS:
    return (hp_tlv_tostring_qwords(st, nd->val.qwordsval.len, nd->val.qwordsval.data));
  case HP_TLV_TYPE_LIST:
    {
      hp_tlv_stream  sub;
      struct hp_list *p;

      if (hp_tlv_tostring_list_begin(st, sub) < 0)  return (-1);

      for (p = HP_LIST_FIRST(nd->val.listval); p != HP_LIST_END(nd->val.listval); p = HP_LIST_NEXT(p)) {
	if (hp_tlv_tostring(sub, FIELD_PTR_TO_STRUCT_PTR(p, struct hp_tlv_node, siblings)) < 0)  return (-1);
      }
      
      return (hp_tlv_tostring_list_end(st, sub));
    }
  default:
    assert(0);
  }

  return (-1);
}


int
hp_tlv_parse_sax(struct hp_tlv_stream *st, enum hp_tlv_type *type, unsigned *data_len, char **pdata)
{
  if (tlv_hdr_get(st, type, data_len) < 0)  return (-1);

  *pdata = tlv_stream_ptr(st);

  tlv_stream_adv(st, *data_len);

  return (0);
}


int
hp_tlv_parse_sax_bool(unsigned data_len, char *data, unsigned *val)
{
  if (data_len != 1)  return (-1);

  *val = (*data == 'T');

  return (0);
}


int
hp_tlv_parse_sax_int(unsigned data_len, char *data, hp_tlv_intval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  memcpy(data_buf, data, data_len);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_INT, val) == 1 ? 0 : -1);
}


int
hp_tlv_parse_sax_float(unsigned data_len, char *data, hp_tlv_floatval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  memcpy(data_buf, data, data_len);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_FLOAT, val) == 1 ? 0 : -1);
}


int
hp_tlv_parse_sax_string(unsigned data_len, char *data, unsigned strbuf_size, char *strbuf)
{
  if ((data_len + 1) > strbuf_size)  return (-1);

  memcpy(strbuf, data, data_len);
  strbuf[data_len] = 0;

  return (0);
}


int
hp_tlv_parse_sax_bytes(unsigned data_len, char *data, unsigned bytes_size, unsigned char *bytes)
{
  unsigned n, nn, val;
  char     data_buf[2 + 1];

  if ((data_len & 1) == 1)  return (-1);

  n = data_len >> 1;
  if (n > bytes_size)  return (-1);

  for (data_buf[2] = 0, nn = n; nn; --nn, ++bytes, data += 2) {
    memcpy(data_buf, data, 2);
    if (sscanf(data_buf, "%02x", &val) != 1)  return (-1);
    *bytes = val;
  }

  return (n);
}


int
hp_tlv_parse_sax_words(unsigned data_len, char *data, unsigned words_size, unsigned short *words)
{
  unsigned n, nn, val;
  char     data_buf[4 + 1];

  if ((data_len & 1) == 1)  return (-1);

  n = data_len >> 2;
  if (n > words_size)  return (-1);

  for (data_buf[4] = 0, nn = n; nn; --nn, ++words, data += 4) {
    memcpy(data_buf, data, 4);
    if (sscanf(data_buf, "%04x", &val) != 1)  return (-1);
    *words = val;
  }

  return (n);
}


int
hp_tlv_parse_sax_dwords(unsigned data_len, char *data, unsigned dwords_size, unsigned long *dwords)
{
  unsigned n, nn, val;
  char     data_buf[8 + 1];

  if ((data_len & 1) == 1)  return (-1);

  n = data_len >> 3;
  if (n > dwords_size)  return (-1);

  for (data_buf[8] = 0, nn = n; nn; --nn, ++dwords, data += 8) {
    memcpy(data_buf, data, 8);
    if (sscanf(data_buf, "%08x", &val) != 1)  return (-1);
    *dwords = val;
  }

  return (n);
}


int
hp_tlv_parse_sax_qwords(unsigned data_len, char *data, unsigned qwords_size, unsigned long long *qwords)
{
  unsigned           n, nn;
  unsigned long long val;
  char               data_buf[16 + 1];

  if ((data_len & 1) == 1)  return (-1);

  n = data_len >> 4;
  if (n > qwords_size)  return (-1);

  for (data_buf[16] = 0, nn = n; nn; --nn, ++qwords, data += 16) {
    memcpy(data_buf, data, 16);
    if (sscanf(data_buf, "%016llx", &val) != 1)  return (-1);
    *qwords = val;
  }

  return (n);
}


int
hp_tlv_parse_dom(struct hp_tlv_stream *st, struct hp_tlv_node **nd)
{
  unsigned type, data_len;
  char     *data;

  if (hp_tlv_parse_sax(st, &type, &data_len, &data) < 0)  return (-1);

  switch (type) {
  case HP_TLV_TYPE_NIL:
    *nd = hp_tlv_node_new(type);
    break;

  case HP_TLV_TYPE_BOOL:
    {
      unsigned val;

      if (hp_tlv_parse_sax_bool(data_len, data, &val) < 0)  return (-1);

      *nd = hp_tlv_node_new(type, val);
    }
    break;

  case HP_TLV_TYPE_INT:
    {
      hp_tlv_intval val;

      if (hp_tlv_parse_sax_int(data_len, data, &val) < 0)  return (-1);

      *nd = hp_tlv_node_new(type, val);
    }
    break;

  case HP_TLV_TYPE_FLOAT:
    {
      hp_tlv_floatval val;

      if (hp_tlv_parse_sax_float(data_len, data, &val) < 0)  return (-1);

      *nd = hp_tlv_node_new(type, val);
    }
    break;

  case HP_TLV_TYPE_STRING:
    {
      unsigned           bufsize;
      char               *buf;
      struct hp_tlv_node *p;

      bufsize = data_len + 1;
      buf     = malloc(bufsize);
      if (buf == 0)  return (-1);

      if (hp_tlv_parse_sax_string(data_len, data, bufsize, buf) < 0)  return (-1);

      *nd = p = tlv_node_new(type);

      p->val.strval = buf;
    }
    break;

  case HP_TLV_TYPE_BYTES:
    {
      unsigned           bufsize;
      unsigned char      *buf;
      struct hp_tlv_node *p;
    
      bufsize = data_len >> 1;
      buf = malloc(bufsize);
      if (buf == 0)  return (-1);

      if (hp_tlv_parse_sax_bytes(data_len, data, bufsize, buf) < 0)  return (-1);

      *nd = p = tlv_node_new(type);

      p->val.bytesval.len  = bufsize;
      p->val.bytesval.data = buf;
    }
    break;

  case HP_TLV_TYPE_WORDS:
    {
      unsigned           bufsize;
      unsigned short     *buf;
      struct hp_tlv_node *p;
    
      bufsize = data_len >> 2;
      buf = malloc(bufsize << 1);
      if (buf == 0)  return (-1);

      if (hp_tlv_parse_sax_words(data_len, data, bufsize, buf) < 0)  return (-1);

      *nd = p = tlv_node_new(type);

      p->val.wordsval.len  = bufsize;
      p->val.wordsval.data = buf;
    }
    break;

  case HP_TLV_TYPE_DWORDS:
    {
      unsigned           bufsize;
      unsigned long      *buf;
      struct hp_tlv_node *p;
    
      bufsize = data_len >> 3;
      buf = malloc(bufsize << 2);
      if (buf == 0)  return (-1);

      if (hp_tlv_parse_sax_dwords(data_len, data, bufsize, buf) < 0)  return (-1);

      *nd = p = tlv_node_new(type);

      p->val.dwordsval.len  = bufsize;
      p->val.dwordsval.data = buf;
    }
    break;

  case HP_TLV_TYPE_QWORDS:
    {
      unsigned           bufsize;
      unsigned long long *buf;
      struct hp_tlv_node *p;
    
      bufsize = data_len >> 4;
      buf = malloc(bufsize << 3);
      if (buf == 0)  return (-1);

      if (hp_tlv_parse_sax_qwords(data_len, data, bufsize, buf) < 0)  return (-1);

      *nd = p = tlv_node_new(type);

      p->val.qwordsval.len  = bufsize;
      p->val.qwordsval.data = buf;
    }
    break;

  case HP_TLV_TYPE_LIST:
    {
      struct hp_tlv_node *p, *child;
      hp_tlv_stream      sub;
      
      *nd = p = hp_tlv_node_new(type);

      for (hp_tlv_stream_init(sub, data_len, data); tlv_stream_rem(sub) != 0; ) {
	if (hp_tlv_parse_dom(sub, &child) < 0)  return (-1);
	hp_tlv_node_child_append(child, p);
      }
    }
    break;
  default:
    assert(0);
  }

  return (0);
}


#ifdef __UNIT_TEST__

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

  assert(hp_tlv_tostring(st, root) == 0);

  dump(buf, tlv_stream_ofs(st));
  printf("\n");

  hp_tlv_stream_init(st, tlv_stream_ofs(st), buf);

  assert(hp_tlv_parse_dom(st, &root2) == 0);

  tlv_dump(root2);
  printf("\n");

  return (0);
}

#endif /* defined(__UNIT_TEST__) */
