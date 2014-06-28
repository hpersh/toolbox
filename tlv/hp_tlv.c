#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "shared/hp_common.h"
#include "hp_tlv.h"


#define BITS_TO_CHARS(x)  (((x) + 3) / 4)

enum {
  HP_TLV_HDR_BITS  = HP_TLV_HDR_TYPE_BITS + HP_TLV_HDR_LEN_BITS, /* TLV header length, in bits */
  HP_TLV_HDR_CHARS = BITS_TO_CHARS(HP_TLV_HDR_BITS)
};

struct hp_tlv_tostring_stream *
hp_tlv_tostring_stream_init(struct hp_tlv_tostring_stream *st, struct hp_stream *iost)
{
  memset(st, 0, sizeof(*st));
  st->iost = iost;

  return (st);
}

static int
hp_tlv_tostring_uint_put(struct hp_tlv_tostring_stream *st, unsigned n, unsigned val)
{
  char buf[9];

  assert(n < ARRAY_SIZE(buf));

  sprintf(buf, "%08x", val);

  return (hp_stream_puts(st->iost, buf + 8 - n));
}

static int
hp_tlv_tostring_uint_get(struct hp_tlv_tostring_stream *st, unsigned n, unsigned *val)
{
  char buf[9];

  assert(n < ARRAY_SIZE(buf));

  if (hp_stream_gets(st->iost, buf, n + 1) != n)  return (-1);

  return (sscanf(buf, "%x", val) == 1 ? n : -1);
}

static int
hp_tlv_tostring_hdr_put(struct hp_tlv_tostring_stream *st, unsigned type, unsigned data_len)
{
  assert(type < (1 << HP_TLV_HDR_TYPE_BITS));
  assert(data_len < (1 << HP_TLV_HDR_LEN_BITS));
  
  return (hp_tlv_tostring_uint_put(st, HP_TLV_HDR_CHARS, (type << HP_TLV_HDR_LEN_BITS) | data_len));
}

static int
hp_tlv_tostring_hdr_get(struct hp_tlv_tostring_stream *st, unsigned *type, unsigned *data_len)
{
  int      n;
  unsigned h;
  
  if ((n = hp_tlv_tostring_uint_get(st, HP_TLV_HDR_CHARS, &h)) < 0)  return (-1);
  
  *type     = h >> HP_TLV_HDR_LEN_BITS;
  *data_len = h & ((1 << HP_TLV_HDR_LEN_BITS) - 1);
  
  return (n);
}


int
hp_tlv_tostring_nil(struct hp_tlv_tostring_stream *st, unsigned type)
{
  return (hp_tlv_tostring_hdr_put(st, type, 0));
}

#define TRYN(x)  do { if ((n = (x)) < 0)  return (-1);  result += n; } while (0)

int
hp_tlv_tostring_bool(struct hp_tlv_tostring_stream *st, unsigned type, unsigned val)
{
  int result = 0, n;

  TRYN(hp_tlv_tostring_hdr_put(st, type, 1));
  TRYN(hp_stream_puts(st->iost, val ? "T" : "F"));

  return (result);
}


int
hp_tlv_tostring_int(struct hp_tlv_tostring_stream *st, unsigned type, hp_tlv_intval val)
{
  int      result = 0, n;
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_INT, val);
  data_len = strlen(data_buf);

  TRYN(hp_tlv_tostring_hdr_put(st, type, data_len));
  TRYN(hp_stream_puts(st->iost, data_buf));

  return (result);
}


int
hp_tlv_tostring_float(struct hp_tlv_tostring_stream *st, unsigned type, hp_tlv_floatval val)
{
  int      result = 0, n;
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_FLOAT, val);
  data_len = strlen(data_buf);

  TRYN(hp_tlv_tostring_hdr_put(st, type, data_len));
  TRYN(hp_stream_puts(st->iost, data_buf));

  return (result);
}


int
hp_tlv_tostring_string(struct hp_tlv_tostring_stream *st, unsigned type, char *s)
{
  int      result = 0, n;
  unsigned data_len = strlen(s);

  TRYN(hp_tlv_tostring_hdr_put(st, type, data_len));
  TRYN(hp_stream_puts(st->iost, s));

  return (result);
}


int
hp_tlv_tostring_bytes(struct hp_tlv_tostring_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len)
{
  int result = 0, n;

  TRYN(hp_tlv_tostring_hdr_put(st, type, 2 * bytes_len));
  for ( ; bytes_len; --bytes_len, ++bytes)  TRYN(hp_tlv_tostring_uint_put(st, 2, *bytes));

  return (result);
}


int
hp_tlv_tostring_words(struct hp_tlv_tostring_stream *st, unsigned type, unsigned short *words, unsigned words_len)
{
  int result = 0, n;

  TRYN(hp_tlv_tostring_hdr_put(st, type, 4 * words_len));
  for ( ; words_len; --words_len, ++words)  TRYN(hp_tlv_tostring_uint_put(st, 4, *words));

  return (result);
}


int
hp_tlv_tostring_dwords(struct hp_tlv_tostring_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len)
{
  int result = 0, n;

  TRYN(hp_tlv_tostring_hdr_put(st, type, 8 * dwords_len));
  for ( ; dwords_len; --dwords_len, ++dwords)  TRYN(hp_tlv_tostring_uint_put(st, 8, *dwords));

  return (result);
}


int
hp_tlv_tostring_qwords(struct hp_tlv_tostring_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len)
{
  int      result = 0, n;
  char     data_buf[16 + 1];

  TRYN(hp_tlv_tostring_hdr_put(st, type, 16 * qwords_len));
  for (; qwords_len; --qwords_len, ++qwords) {
    sprintf(data_buf, "%016llx", *qwords);
    TRYN(hp_stream_puts(st->iost, data_buf));
  }

  return (result);
}


int
hp_tlv_tostring_list_begin(struct hp_tlv_tostring_stream *st, unsigned type, struct hp_tlv_tostring_stream *cst)
{
  cst->parent  = st;
  cst->hdr_ofs = hp_stream_tell(cst->iost = st->iost);

  return (hp_tlv_tostring_hdr_put(cst, type, 0));
}


int
hp_tlv_tostring_list_end(struct hp_tlv_tostring_stream *st)
{
  unsigned cur_ofs, type, odata_len;

  return (st->parent == 0
	  || (cur_ofs = hp_stream_tell(st->iost)) < 0
	  || hp_stream_seek(st->iost, st->hdr_ofs, SEEK_SET) < 0
	  || hp_tlv_tostring_hdr_get(st, &type, &odata_len) < 0
	  || hp_stream_seek(st->iost, st->hdr_ofs, SEEK_SET) < 0
	  || hp_tlv_tostring_hdr_put(st, type, cur_ofs - (st->hdr_ofs + HP_TLV_HDR_CHARS)) < 0
	  || hp_stream_seek(st->iost, cur_ofs, SEEK_SET) < 0
	  ? -1 : 0
	  );
}




struct hp_tlv_parse_stream *
hp_tlv_parse_stream_init(struct hp_tlv_parse_stream *st, struct hp_stream *iost)
{
  memset(st, 0, sizeof(*st));
  st->iost = iost;

  return (st);
}


static int
hp_tlv_parse_stream_getc(struct hp_tlv_parse_stream *st)
{
  int result;

  if (st->parent) {
    if (st->rem == 0)  return (-1);
  }

  result = hp_stream_getc(st->iost);
  
  if (st->parent) {
    if (result >= 0 && st->rem > 0)  --st->rem;
  }

  return (result);
}

static inline int
hp_tlv_parse_stream_gets(struct hp_tlv_parse_stream *st, char *buf, unsigned bufsize)
{
  int      result;
  unsigned n;

  assert(bufsize > 0);

  n = bufsize - 1;

  if (st->parent) {
    if (n > st->rem)  n = st->rem;
  }

  result = hp_stream_gets(st->iost, buf, n + 1);

  if (st->parent) {
    if (result >= 0)  st->rem -= result;
  }

  return (result);
}

static int
hp_tlv_parse_uint_get(struct hp_tlv_parse_stream *st, unsigned n, unsigned *val)
{
  char buf[9];

  assert(n < ARRAY_SIZE(buf));

  if (hp_tlv_parse_stream_gets(st, buf, n + 1) != n)  return (-1);

  return (sscanf(buf, "%x", val) == 1 ? n : -1);
}


int
hp_tlv_parse_hdr(struct hp_tlv_parse_stream *st, unsigned *type, unsigned *data_len)
{
  int      n;
  unsigned h;
  
  if ((n = hp_tlv_parse_uint_get(st, HP_TLV_HDR_CHARS, &h)) < 0)  return (-1);
  
  *type     = h >> HP_TLV_HDR_LEN_BITS;
  *data_len = h & ((1 << HP_TLV_HDR_LEN_BITS) - 1);
  
  return (n);
}


int
hp_tlv_parse_bool(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned *val)
{
  char data_buf[2];
  
  if (data_len != 1
      || hp_tlv_parse_stream_gets(st, data_buf, 1) != 1
      ) {
    return (-1);
  }

  switch (data_buf[0]) {
  case 'T':
    *val = 1;
    break;
  case 'F':
    *val = 0;
    break;
  default:
    return (-1);
  }

  return (1);
}


int
hp_tlv_parse_int(struct hp_tlv_parse_stream *st, unsigned data_len, hp_tlv_intval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  return (hp_tlv_parse_stream_gets(st, data_buf, data_len + 1) != data_len
	  || sscanf(data_buf, HP_TLV_FMT_INT, val) != 1
	  ? -1 : data_len
	  );
}


int
hp_tlv_parse_float(struct hp_tlv_parse_stream *st, unsigned data_len, hp_tlv_floatval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  return (hp_tlv_parse_stream_gets(st, data_buf, data_len + 1) != data_len
	  || sscanf(data_buf, HP_TLV_FMT_FLOAT, val) != 1
	  ? -1 : data_len
	  );
}


int
hp_tlv_parse_string(struct hp_tlv_parse_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size)
{
  return (data_len >= strbuf_size
	  || hp_tlv_parse_stream_gets(st, strbuf, data_len + 1) != data_len
	  ? -1 : data_len
	  );
}


int
hp_tlv_parse_bytes(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size)
{
  unsigned n, val;

  if ((data_len & 1) != 0 || (n = data_len >> 1) > bytes_size)  return (-1);

  for ( ; n; --n, ++bytes) {
    if (hp_tlv_parse_uint_get(st, 2, &val) != 2)  return (-1);
    *bytes = val;
  }

  return (data_len);
}


int
hp_tlv_parse_words(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned short *words, unsigned words_size)
{
  unsigned n, val;

  if ((data_len & 3) != 0 || (n = data_len >> 2) > words_size)  return (-1);

  for ( ; n; --n, ++words) {
    if (hp_tlv_parse_uint_get(st, 4, &val) != 4)  return (-1);
    *words = val;
  }

  return (data_len);
}


int
hp_tlv_parse_dwords(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size)
{
  unsigned n, val;

  if ((data_len & 7) != 0 || (n = data_len >> 3) > dwords_size)  return (-1);

  for ( ; n; --n, ++dwords) {
    if (hp_tlv_parse_uint_get(st, 8, &val) != 8)  return (-1);
    *dwords = val;
  }

  return (data_len);
}


int
hp_tlv_parse_qwords(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size)
{
  unsigned           n;
  unsigned long long val;
  char               data_buf[16 + 1];

  if ((data_len & 0xf) != 0 || (n = data_len >> 4) > qwords_size)  return (-1);

  for ( ; n; --n, ++qwords) {
    if (hp_tlv_parse_stream_gets(st, data_buf, sizeof(data_buf)) != (sizeof(data_buf) - 1))  return (-1);
    if (sscanf(data_buf, "%016llx", &val) != 1)  return (-1);
    *qwords = val;
  }    

  return (data_len);
}


int
hp_tlv_parse_list_begin(struct hp_tlv_parse_stream *st, unsigned data_len, struct hp_tlv_parse_stream *cst)
{
  cst->parent = st;
  cst->iost   = st->iost;
  cst->data_len = cst->rem = data_len;

  return (0);
}


int
hp_tlv_parse_list_end(struct hp_tlv_parse_stream *st)
{
  struct hp_tlv_parse_stream *p = st->parent;

  assert(p);

  if (st->rem == 0) {
    if (p->rem > 0) {
      assert(p->rem >= st->data_len);
      p->rem -= st->data_len;
    }
    return (1);
  }

  return (0);
}
