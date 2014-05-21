#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "hp_tlv.h"


#define BITS_TO_CHARS(x)  (((x) + 3) / 4)

enum {
  HP_TLV_HDR_BITS  = HP_TLV_HDR_TYPE_BITS + HP_TLV_HDR_LEN_BITS, /* TLV header length, in bits */
  HP_TLV_HDR_CHARS = BITS_TO_CHARS(HP_TLV_HDR_BITS)
};


void
hp_tlv_stream_init(struct hp_tlv_stream *st,
		   int (*putc)(struct hp_tlv_stream *st, char c),
		   int (*getc)(struct hp_tlv_stream *st),
		   int (*tell)(struct hp_tlv_stream *st, unsigned *ofs),
		   int (*seek)(struct hp_tlv_stream *st, unsigned ofs)
		   )
{
  st->putc = putc;
  st->getc = getc;
  st->tell = tell;
  st->seek = seek;
}

static int
tlv_stream_putc(struct hp_tlv_stream *st, char c)
{
  return ((*st->putc)(st, c) < 0 ? -1 : 1);
}

static int
tlv_stream_puts(struct hp_tlv_stream *st, char *p, unsigned n)
{
  unsigned nn;
  
  for (nn = n; nn; --nn, ++p) {
    if (tlv_stream_putc(st, *p) < 0)  return (-1);
  }

  return (n);
}

static int
tlv_stream_getc(struct hp_tlv_stream *st)
{
  return ((*st->getc)(st) < 0 ? -1 : 1);
}

static int
tlv_stream_gets(struct hp_tlv_stream *st, char *p, unsigned n)
{
  unsigned nn;
  int      c;

  for (nn = n; nn; --nn, ++p) {
    if ((c = (*st->getc)(st)) < 0)  return (-1);
    *p = c;
  }

  return (n);
}

static int
tlv_stream_tell(struct hp_tlv_stream *st, unsigned *ofs)
{
  return ((*st->tell)(st, ofs));
}

static int
tlv_stream_seek(struct hp_tlv_stream *st, unsigned ofs)
{
  return ((*st->seek)(st, ofs));
}

static int
tlv_stream_uint_put(struct hp_tlv_stream *st, unsigned n, unsigned val)
{
  char buf[9], *p;
  int  nn;

  assert(n < ARRAY_SIZE(buf));

  sprintf(buf, "%08x", val);
  return (tlv_stream_puts(st, buf + 8 - n, n));
}

static int
tlv_stream_uint_get(struct hp_tlv_stream *st, unsigned n, unsigned *val)
{
  char buf[9];

  assert(n < ARRAY_SIZE(buf));

  if (tlv_stream_gets(st, buf, n) < 0)  return (-1);
  buf[n] = 0;

  return (sscanf(buf, "%x", val) == 1 ? n : -1);
}

static int
tlv_hdr_put(struct hp_tlv_stream *st, unsigned type, unsigned data_len)
{
  assert(type < (1 << HP_TLV_HDR_TYPE_BITS));
  assert(data_len < (1 << HP_TLV_HDR_LEN_BITS));
  
  return (tlv_stream_uint_put(st, HP_TLV_HDR_CHARS, (type << HP_TLV_HDR_LEN_BITS) | data_len));
}

static int
tlv_hdr_get(struct hp_tlv_stream *st, unsigned *type, unsigned *data_len)
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
  return (tlv_hdr_put(st, type, 0));
}


int
hp_tlv_tostring_bool(struct hp_tlv_stream *st, unsigned type, unsigned val)
{
  int result, n;

  if ((n = tlv_hdr_put(st, type, 1)) < 0)  return (-1);
  result = n;
  if ((n = tlv_stream_putc(st, val ? 'T' : 'F')) < 0)  return (-1);
  result += n;
      
  return (result);
}


int
hp_tlv_tostring_int(struct hp_tlv_stream *st, unsigned type, int val)
{
  int      result, n;
  char     data_buf[32];
  unsigned data_len;
  
  sprintf(data_buf, HP_TLV_FMT_INT, val);
  data_len = strlen(data_buf);

  if ((n = tlv_hdr_put(st, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = tlv_stream_puts(st, data_buf, data_len)) < 0)  return (-1);
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

  if ((n = tlv_hdr_put(st, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = tlv_stream_puts(st, data_buf, data_len)) < 0)  return (-1);
  result += n;

  return (result);
}


int
hp_tlv_tostring_string(struct hp_tlv_stream *st, unsigned type, char *s)
{
  int      result, n;
  unsigned data_len = strlen(s);

  if ((n = tlv_hdr_put(st, type, data_len)) < 0)  return (-1);
  result = n;
  if ((n = tlv_stream_puts(st, s, data_len)) < 0)  return (-1);
  result += n;

  return (result);
}


int
hp_tlv_tostring_bytes(struct hp_tlv_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st, type, 2 * bytes_len)) < 0)  return (-1);
  result = n;
  for ( ; bytes_len; --bytes_len, ++bytes) {
    if ((n = tlv_stream_uint_put(st, 2, *bytes)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_words(struct hp_tlv_stream *st, unsigned type, unsigned short *words, unsigned words_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st, type, 4 * words_len)) < 0)  return (-1);
  result = n;
  for ( ; words_len; --words_len, ++words) {
    if ((n = tlv_stream_uint_put(st, 4, *words)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_dwords(struct hp_tlv_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len)
{
  int result, n;

  if ((n = tlv_hdr_put(st, type, 8 * dwords_len)) < 0)  return (-1);
  result = n;
  for ( ; dwords_len; --dwords_len, ++dwords) {
    if ((n = tlv_stream_uint_put(st, 8, *dwords)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_qwords(struct hp_tlv_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len)
{
  int      result, n;
  char     data_buf[16 + 1];

  if ((n = tlv_hdr_put(st, type, 16 * qwords_len)) < 0)  return (-1);
  result = n;
  for (; qwords_len; --qwords_len, ++qwords) {
    sprintf(data_buf, "%016llx", *qwords);
    if ((n = tlv_stream_puts(st, data_buf, 16)) < 0)  return (-1);
    result += n;
  }

  return (result);
}


int
hp_tlv_tostring_list_begin(struct hp_tlv_stream *st, unsigned type, unsigned *hofs)
{
  if (tlv_stream_tell(st, hofs) < 0)  return (-1);

  return (tlv_hdr_put(st, type, 0));
}


int
hp_tlv_tostring_list_end(struct hp_tlv_stream *st, unsigned type, unsigned hofs)
{
  unsigned ofs, otype, odata_len;

  return (tlv_stream_tell(st, &ofs) < 0
	  || tlv_stream_seek(st, hofs) < 0
	  || tlv_hdr_get(st, &otype, &odata_len) < 0
	  || type != otype
	  || tlv_stream_seek(st, hofs) < 0
	  || tlv_hdr_put(st, type, ofs - (hofs + HP_TLV_HDR_CHARS)) < 0
	  || tlv_stream_seek(st, ofs) < 0
	  ? -1 : 0
	  );
}


int
hp_tlv_parse_sax_hdr(struct hp_tlv_stream *st, unsigned *type, unsigned *data_len)
{
  return (tlv_hdr_get(st, type, data_len));
}


int
hp_tlv_parse_sax_bool(struct hp_tlv_stream *st, unsigned data_len, unsigned *val)
{
  char c;

  if (data_len != 1)  return (-1);

  if ((c = tlv_stream_getc(st)) < 0)  return (-1);

  *val = (c == 'T');

  return (data_len);
}


int
hp_tlv_parse_sax_int(struct hp_tlv_stream *st, unsigned data_len, hp_tlv_intval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  if (tlv_stream_gets(st, data_buf, data_len) < 0)  return (-1);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_INT, val) == 1 ? data_len : -1);
}


int
hp_tlv_parse_sax_float(struct hp_tlv_stream *st, unsigned data_len, hp_tlv_floatval *val)
{
  char data_buf[32];
  
  if (data_len == 0)  return (-1);

  assert(data_len < ARRAY_SIZE(data_buf));

  if (tlv_stream_gets(st, data_buf, data_len) < 0)  return (-1);
  data_buf[data_len] = 0;
  return (sscanf(data_buf, HP_TLV_FMT_FLOAT, val) == 1 ? data_len : -1);
}


int
hp_tlv_parse_sax_string(struct hp_tlv_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size)
{
  if (data_len >= strbuf_size)  return (-1);

  if (tlv_stream_gets(st, strbuf, data_len) < 0)  return (-1);
  strbuf[data_len] = 0;

  return (data_len);
}


int
hp_tlv_parse_sax_bytes(struct hp_tlv_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size)
{
  unsigned n, val;

  if ((data_len & 1) == 1 && (n = data_len >> 1) > bytes_size)  return (-1);

  for ( ; n; --n, ++bytes) {
    if (tlv_stream_uint_get(st, 2, &val) < 0)  return (-1);
    *bytes = val;
  }

  return (data_len);
}


int
hp_tlv_parse_sax_words(struct hp_tlv_stream *st, unsigned data_len, unsigned short *words, unsigned words_size)
{
  unsigned n, val;

  if ((data_len & 1) == 1 && (n = data_len >> 2) > words_size)  return (-1);

  for ( ; n; --n, ++words) {
    if (tlv_stream_uint_get(st, 4, &val) < 0)  return (-1);
    *words = val;
  }

  return (data_len);
}


int
hp_tlv_parse_sax_dwords(struct hp_tlv_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size)
{
  unsigned n, val;

  if ((data_len & 1) == 1 && (n = data_len >> 3) > dwords_size)  return (-1);

  for ( ; n; --n, ++dwords) {
    if (tlv_stream_uint_get(st, 8, &val) < 0)  return (-1);
    *dwords = val;
  }

  return (data_len);
}


int
hp_tlv_parse_sax_qwords(struct hp_tlv_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size)
{
  unsigned           n;
  unsigned long long val;
  char               data_buf[16 + 1];

  if ((data_len & 1) == 1 && (n = data_len >> 4) > qwords_size)  return (-1);

  for ( ; n; --n, ++qwords) {
    if (tlv_stream_gets(st, data_buf, 16) < 0)  return (-1);
    data_buf[16] = 0;
    if (sscanf(data_buf, "%016llx", &val) != 1)  return (-1);
    *qwords = val;
  }    

  return (data_len);
}
