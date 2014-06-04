#ifndef __HP_TLV_H
#define __HP_TLV_H

#include "common/hp_common.h"
#include "stream/hp_stream.h"

#include "hp_tlv_cfg.h"

int hp_tlv_tostring_nil(struct hp_stream *st, unsigned type);
int hp_tlv_tostring_bool(struct hp_stream *st, unsigned type, unsigned val);
int hp_tlv_tostring_int(struct hp_stream *st, unsigned type, int val);
int hp_tlv_tostring_float(struct hp_stream *st, unsigned type, double val);
int hp_tlv_tostring_string(struct hp_stream *st, unsigned type, char *s);
int hp_tlv_tostring_bytes(struct hp_stream *st, unsigned type, unsigned char *bytes, unsigned bytes_len);
int hp_tlv_tostring_words(struct hp_stream *st, unsigned type, unsigned short *words, unsigned words_len);
int hp_tlv_tostring_dwords(struct hp_stream *st, unsigned type, unsigned long *dwords, unsigned dwords_len);
int hp_tlv_tostring_qwords(struct hp_stream *st, unsigned type, unsigned long long *qwords, unsigned qwords_len);
int hp_tlv_tostring_list_begin(struct hp_stream *st, unsigned type, unsigned *hofs);
int hp_tlv_tostring_list_end(struct hp_stream *st, unsigned type, unsigned hofs);

int hp_tlv_parse_sax_hdr(struct hp_stream *st, unsigned *type, unsigned *data_len);
int hp_tlv_parse_sax_bool(struct hp_stream *st, unsigned data_len, unsigned *val);
int hp_tlv_parse_sax_int(struct hp_stream *st, unsigned data_len, hp_tlv_intval *val);
int hp_tlv_parse_sax_float(struct hp_stream *st, unsigned data_len, hp_tlv_floatval *val);
int hp_tlv_parse_sax_string(struct hp_stream *st, unsigned data_len, char *strbuf, unsigned strbuf_size);
int hp_tlv_parse_sax_bytes(struct hp_stream *st, unsigned data_len, unsigned char *bytes, unsigned bytes_size);
int hp_tlv_parse_sax_words(struct hp_stream *st, unsigned data_len, unsigned short *words, unsigned words_size);
int hp_tlv_parse_sax_dwords(struct hp_stream *st, unsigned data_len, unsigned long *dwords, unsigned dwords_size);
int hp_tlv_parse_sax_qwords(struct hp_stream *st, unsigned data_len, unsigned long long *qwords, unsigned qwords_size);

#endif /* !defined(__HP_TLV_H) */
