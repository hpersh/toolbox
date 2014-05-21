#ifndef __HP_TLV_H
#define __HP_TLV_H

#include "common/hp_common.h"

#include "hp_tlv_cfg.h"

struct hp_tlv_stream {
  int      (*putc)(struct hp_tlv_stream *st, char c);
  int      (*getc)(struct hp_tlv_stream *st);
  int      (*tell)(struct hp_tlv_stream *st, unsigned *ofs);
  int      (*seek)(struct hp_tlv_stream *st, unsigned ofs);
};
typedef struct hp_tlv_stream hp_tlv_stream[1]; 

void
hp_tlv_stream_init(struct hp_tlv_stream *st,
		   int (*putc)(struct hp_tlv_stream *st, char c),
		   int (*getc)(struct hp_tlv_stream *st),
		   int (*tell)(struct hp_tlv_stream *st, unsigned *ofs),
		   int (*seek)(struct hp_tlv_stream *st, unsigned ofs)
		   );

int hp_tlv_tostring_nil(struct hp_tlv_stream *st, unsigned type);
int hp_tlv_tostring_bool(struct hp_tlv_stream *st, unsigned type, unsigned val);
int hp_tlv_tostring_int(struct hp_tlv_stream *st, unsigned type, int val);
int hp_tlv_tostring_float(struct hp_tlv_stream *st, unsigned type, double val);
int hp_tlv_tostring_string(struct hp_tlv_stream *st, unsigned type, char *s);
int hp_tlv_tostring_bytes(struct hp_tlv_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len);
int hp_tlv_tostring_words(struct hp_tlv_stream *st, unsigned type, unsigned short *words, unsigned words_len);
int hp_tlv_tostring_dwords(struct hp_tlv_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len);
int hp_tlv_tostring_qwords(struct hp_tlv_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len);
int hp_tlv_tostring_list_begin(struct hp_tlv_stream *st, unsigned type, unsigned *hofs);
int hp_tlv_tostring_list_end(struct hp_tlv_stream *st, unsigned type, unsigned hofs);

int hp_tlv_parse_sax_hdr(struct hp_tlv_stream *st, unsigned *type, unsigned *data_len);
int hp_tlv_parse_sax_bool(struct hp_tlv_stream *st, unsigned data_len, unsigned *val);
int hp_tlv_parse_sax_int(struct hp_tlv_stream *st, unsigned data_len, hp_tlv_intval *val);
int hp_tlv_parse_sax_float(struct hp_tlv_stream *st, unsigned data_len, hp_tlv_floatval *val);
int hp_tlv_parse_sax_string(struct hp_tlv_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size);
int hp_tlv_parse_sax_bytes(struct hp_tlv_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size);
int hp_tlv_parse_sax_words(struct hp_tlv_stream *st, unsigned data_len, unsigned short *words, unsigned words_size);
int hp_tlv_parse_sax_dwords(struct hp_tlv_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size);
int hp_tlv_parse_sax_qwords(struct hp_tlv_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size);

#endif /* !defined(__HP_TLV_H) */
