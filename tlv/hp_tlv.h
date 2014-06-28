#ifndef __HP_TLV_H
#define __HP_TLV_H

#include "stream/hp_stream.h"

#include "hp_tlv_cfg.h"

struct hp_tlv_stream {
};

struct hp_tlv_tostring_stream {
  struct hp_tlv_tostring_stream *parent;
  struct hp_stream              *iost;
  unsigned                      hdr_ofs;
};

struct hp_tlv_tostring_stream *hp_tlv_tostring_stream_init(struct hp_tlv_tostring_stream *st, struct hp_stream *iost);

int hp_tlv_tostring_nil(struct hp_tlv_tostring_stream *st, unsigned type);
int hp_tlv_tostring_bool(struct hp_tlv_tostring_stream *st, unsigned type, unsigned val);
int hp_tlv_tostring_int(struct hp_tlv_tostring_stream *st, unsigned type, hp_tlv_intval val);
int hp_tlv_tostring_float(struct hp_tlv_tostring_stream *st, unsigned type, hp_tlv_floatval val);
int hp_tlv_tostring_string(struct hp_tlv_tostring_stream *st, unsigned type, char *s);
int hp_tlv_tostring_bytes(struct hp_tlv_tostring_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len);
int hp_tlv_tostring_words(struct hp_tlv_tostring_stream *st, unsigned type, unsigned short *words, unsigned words_len);
int hp_tlv_tostring_dwords(struct hp_tlv_tostring_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len);
int hp_tlv_tostring_qwords(struct hp_tlv_tostring_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len);
int hp_tlv_tostring_list_begin(struct hp_tlv_tostring_stream *st, unsigned type, struct hp_tlv_tostring_stream *cst);
int hp_tlv_tostring_list_end(struct hp_tlv_tostring_stream *st);

struct hp_tlv_parse_stream {
  struct hp_tlv_parse_stream *parent;
  struct hp_stream           *iost;
  unsigned                   data_len;    
  unsigned                   rem;
};

struct hp_tlv_parse_stream *hp_tlv_parse_stream_init(struct hp_tlv_parse_stream *st, struct hp_stream *iost);

int hp_tlv_parse_hdr(struct hp_tlv_parse_stream *st, unsigned *type, unsigned *data_len);
int hp_tlv_parse_bool(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned *val);
int hp_tlv_parse_int(struct hp_tlv_parse_stream *st, unsigned data_len, hp_tlv_intval *val);
int hp_tlv_parse_float(struct hp_tlv_parse_stream *st, unsigned data_len, hp_tlv_floatval *val);
int hp_tlv_parse_string(struct hp_tlv_parse_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size);
int hp_tlv_parse_bytes(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size);
int hp_tlv_parse_words(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned short *words, unsigned words_size);
int hp_tlv_parse_dwords(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size);
int hp_tlv_parse_qwords(struct hp_tlv_parse_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size);
int hp_tlv_parse_list_begin(struct hp_tlv_parse_stream *st, unsigned data_len, struct hp_tlv_parse_stream *cst);
int hp_tlv_parse_list_end(struct hp_tlv_parse_stream *st);

#endif /* !defined(__HP_TLV_H) */
