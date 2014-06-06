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


struct hp_tlv_stream *
hp_tlv_stream_init(struct hp_tlv_stream *st, struct hp_stream *iost)
{
  st->parent  = 0;
  st->iost    = iost;
  st->hdr_ofs = 0;

  return (st);
}


static int
tlv_stream_uint_put(struct hp_stream *st, unsigned n, unsigned val)
{
  char buf[9], *p;
  int  nn;

  assert(n < ARRAY_SIZE(buf));

  sprintf(buf, "%08x", val);

  return (hp_stream_puts(st, buf + 8 - n));
}

static int
tlv_stream_uint_get(struct hp_stream *st, unsigned n, unsigned *val)
{
  char buf[9];

  assert(n < ARRAY_SIZE(buf));

  if (hp_stream_gets(st, buf, n + 1) < 0)  return (-1);

  return (sscanf(buf, "%x", val) == 1 ? n : -1);
}

static int
tlv_hdr_put(struct hp_stream *st, unsigned type, unsigned data_len)
{
  assert(type < (1 << HP_TLV_HDR_TYPE_BITS));
  assert(data_len < (1 << HP_TLV_HDR_LEN_BITS));
  
  return (tlv_stream_uint_put(st, HP_TLV_HDR_CHARS, (type << HP_TLV_HDR_LEN_BITS) | data_len));
}

static int
tlv_hdr_get(struct hp_stream *st, unsigned *type, unsigned *data_len)
{
  int      n;
  unsigned h;
  
  if ((n = tlv_stream_uint_get(st, HP_TLV_HDR_CHARS, &h)) < 0)  return (-1);
  
  *type     = h >> HP_TLV_HDR_LEN_BITS;
  *data_len = h & ((1 << HP_TLV_HDR_LEN_BITS) - 1);
  
  return (n);
}


int
hp_tlv_tostring_nil(struct hp_tlv_stream *st, unsigned type)
{
  return (tlv_hdr_put(st->iost, type, 0));
}


int
hp_tlv_tostring_int(struct hp_tlv_stream *st, unsigned type, int val)
{
  int      result, n;
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_INT, val);
  data_len = strlen(data_buf);

  if ((n = tlv_hdr_put(st->iost, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = hp_stream_puts(st->iost, data_buf)) < 0)  return (-1);
  result += n;

  return (result);
}


int
hp_tlv_tostring_float(struct hp_tlv_stream *st, unsigned type, double val)
{
  int      result, n;
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_FLOAT, val);
  data_len = strlen(data_buf);

  if ((n = tlv_hdr_put(st->iost, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = hp_stream_puts(st->iost, data_buf)) < 0)  return (-1);
  result += n;

  return (result);
}


int
hp_tlv_tostring_string(struct hp_tlv_stream *st, unsigned type, char *s)
{
  int      result, n;
  unsigned data_len = strlen(s);

  if ((n = tlv_hdr_put(st->iost, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = hp_stream_puts(st->iost, s)) < 0)  return (-1);
  result += n;

  return (result);
}


int
hp_tlv_tostring_bytes(struct hp_tlv_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st->iost, type, 2 * bytes_len)) < 0)  return (-1);
  result = n;
  for ( ; bytes_len; --bytes_len, ++bytes) {
    if ((n = tlv_stream_uint_put(st->iost, 2, *bytes)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_words(struct hp_tlv_stream *st, unsigned type, unsigned short *words, unsigned words_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st->iost, type, 4 * words_len)) < 0)  return (-1);
  result = n;
  for ( ; words_len; --words_len, ++words) {
    if ((n = tlv_stream_uint_put(st->iost, 4, *words)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_dwords(struct hp_tlv_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st->iost, type, 8 * dwords_len)) < 0)  return (-1);
  result = n;
  for ( ; dwords_len; --dwords_len, ++dwords) {
    if ((n = tlv_stream_uint_put(st->iost, 8, *dwords)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_qwords(struct hp_tlv_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len)
{
  int      result, n;
  char     data_buf[16 + 1];

  if ((n = tlv_hdr_put(st->iost, type, 16 * qwords_len)) < 0)  return (-1);
  result = n;
  for (; qwords_len; --qwords_len, ++qwords) {
    sprintf(data_buf, "%016llx", *qwords);
    if ((n = hp_stream_puts(st->iost, data_buf)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_list_begin(struct hp_tlv_stream *st, unsigned type, struct hp_tlv_stream *cst)
{
  cst->iost    = st->iost;
  cst->parent  = st;
  cst->hdr_ofs = hp_stream_tell(st->iost);

  return (tlv_hdr_put(st->iost, type, 0));
}


int
hp_tlv_tostring_list_end(struct hp_tlv_stream *st)
{
  unsigned cur_ofs, type, odata_len;

  return (st->parent == 0
	  || (cur_ofs = hp_stream_tell(st->iost)) < 0
	  || hp_stream_seek(st->iost, st->hdr_ofs, SEEK_SET) < 0
	  || tlv_hdr_get(st->iost, &type, &odata_len) < 0
	  || hp_stream_seek(st->iost, st->hdr_ofs, SEEK_SET) < 0
	  || tlv_hdr_put(st->iost, type, cur_ofs - (st->hdr_ofs + HP_TLV_HDR_CHARS)) < 0
	  || hp_stream_seek(st->iost, cur_ofs, SEEK_SET) < 0
	  ? -1 : 0
	  );
}


int
hp_tlv_parse_hdr(struct hp_stream *st, unsigned *type, unsigned *data_len)
{
  return (tlv_hdr_get(st, type, data_len));
}


int
hp_tlv_parse_int(struct hp_stream *st, unsigned data_len, hp_tlv_intval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  if (hp_stream_gets(st, data_buf, data_len + 1) < 0)  return (-1);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_INT, val) == 1 ? data_len : -1);
}


int
hp_tlv_parse_float(struct hp_stream *st, unsigned data_len, hp_tlv_floatval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  if (hp_stream_gets(st, data_buf, data_len + 1) < 0)  return (-1);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_FLOAT, val) == 1 ? data_len : -1);
}


int
hp_tlv_parse_string(struct hp_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size)
{
  if (data_len >= strbuf_size)  return (-1);

  if (hp_stream_gets(st, strbuf, data_len + 1) < 0)  return (-1);
  strbuf[data_len] = 0;

  return (data_len);
}


int
hp_tlv_parse_bytes(struct hp_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size)
{
  unsigned n, val;

  if ((data_len & 1) != 0 || (n = data_len >> 1) > bytes_size)  return (-1);

  for ( ; n; --n, ++bytes) {
    if (tlv_stream_uint_get(st, 2, &val) < 0)  return (-1);
    *bytes = val;
  }

  return (data_len);
}


int
hp_tlv_parse_words(struct hp_stream *st, unsigned data_len, unsigned short *words, unsigned words_size)
{
  unsigned n, val;

  if ((data_len & 3) != 0 || (n = data_len >> 2) > words_size)  return (-1);

  for ( ; n; --n, ++words) {
    if (tlv_stream_uint_get(st, 4, &val) < 0)  return (-1);
    *words = val;
  }

  return (data_len);
}


int
hp_tlv_parse_dwords(struct hp_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size)
{
  unsigned n, val;

  if ((data_len & 7) != 0 || (n = data_len >> 3) > dwords_size)  return (-1);

  for ( ; n; --n, ++dwords) {
    if (tlv_stream_uint_get(st, 8, &val) < 0)  return (-1);
    *dwords = val;
  }

  return (data_len);
}


int
hp_tlv_parse_qwords(struct hp_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size)
{
  unsigned           n;
  unsigned long long val;
  char               data_buf[16 + 1];

  if ((data_len & 0xf) != 0 || (n = data_len >> 4) > qwords_size)  return (-1);

  for ( ; n; --n, ++qwords) {
    if (hp_stream_gets(st, data_buf, sizeof(data_buf)) < 0)  return (-1);
    data_buf[16] = 0;
    if (sscanf(data_buf, "%016llx", &val) != 1)  return (-1);
    *qwords = val;
  }    

  return (data_len);
}
