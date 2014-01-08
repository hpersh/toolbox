#ifndef __HP_TLV_H
#define __HP_TLV_H

#include "hp_tlv_cfg.h"

enum hp_tlv_type {
  HP_TLV_TYPE_NIL,
  HP_TLV_TYPE_BOOL,
  HP_TLV_TYPE_INT,
  HP_TLV_TYPE_FLOAT,
  HP_TLV_TYPE_STRING,
  HP_TLV_TYPE_BYTES,
  HP_TLV_TYPE_WORDS,
  HP_TLV_TYPE_DWORDS,
  HP_TLV_TYPE_QWORDS,
  HP_TLV_TYPE_LIST
};

struct hp_tlv_node {
  struct hp_tlv_node *parent;
  hp_list            siblings;

  enum hp_tlv_type type;
  union {
    unsigned char   boolval;
    hp_tlv_intval   intval;
    hp_tlv_floatval floatval;
    char            *strval;
    struct {
      unsigned      len;
      unsigned char *data;
    } bytesval;
    struct {
      unsigned       len;
      unsigned short *data;
    } wordsval;
    struct {
      unsigned      len;
      unsigned long *data;
    } dwordsval;
    struct {
      unsigned           len;
      unsigned long long *data;
    } qwordsval;
    hp_list         listval;
  } val;
};
typedef struct hp_tlv_node hp_tlv_node[1];

struct hp_tlv_node *hp_tlv_node_new(enum hp_tlv_type type, ...);
void               hp_tlv_node_delete(struct hp_tlv_node *nd);
struct hp_tlv_node *hp_tlv_node_child_insert(struct hp_tlv_node *nd, struct hp_tlv_node *parent, struct hp_list *before);
struct hp_tlv_node *hp_tlv_node_child_append(struct hp_tlv_node *nd, struct hp_tlv_node *parent);
struct hp_tlv_node *hp_tlv_node_erase(struct hp_tlv_node *nd);

struct hp_tlv_stream {
  unsigned char *buf;
  unsigned      size;
  unsigned      ofs;
};
typedef struct hp_tlv_stream hp_tlv_stream[1]; 

void hp_tlv_stream_init(struct hp_tlv_stream *st, unsigned len, unsigned char *buf);

int hp_tlv_tostring_nil(struct hp_tlv_stream *st);
int hp_tlv_tostring_bool(struct hp_tlv_stream *st, unsigned val);
int hp_tlv_tostring_int(struct hp_tlv_stream *st, int val);
int hp_tlv_tostring_float(struct hp_tlv_stream *st, double val);
int hp_tlv_tostring_string(struct hp_tlv_stream *st, char *s);
int hp_tlv_tostring_bytes(struct hp_tlv_stream *st, unsigned bytes_len, unsigned char *bytes);
int hp_tlv_tostring_words(struct hp_tlv_stream *st, unsigned words_len, unsigned short *words);
int hp_tlv_tostring_dwords(struct hp_tlv_stream *st, unsigned dwords_len, unsigned long *dwords);
int hp_tlv_tostring_qwords(struct hp_tlv_stream *st, unsigned qwords_len, unsigned long long *qwords);
int hp_tlv_tostring_list_begin(struct hp_tlv_stream *st, struct hp_tlv_stream *sub);
int hp_tlv_tostring_list_end(struct hp_tlv_stream *st, struct hp_tlv_stream *sub);
int hp_tlv_tostring(struct hp_tlv_stream *st, struct hp_tlv_node *nd);

int hp_tlv_parse_sax(struct hp_tlv_stream *st, enum hp_tlv_type *type, unsigned *data_len, char **pdata);
int hp_tlv_parse_sax_bool(unsigned data_len, char *data, unsigned *val);
int hp_tlv_parse_sax_int(unsigned data_len, char *data, hp_tlv_intval *val);
int hp_tlv_parse_sax_float(unsigned data_len, char *data, hp_tlv_floatval *val);
int hp_tlv_parse_sax_string(unsigned data_len, char *data, unsigned strbuf_size, char *strbuf);
int hp_tlv_parse_sax_bytes(unsigned data_len, char *data, unsigned bytes_size, unsigned char *bytes);
int hp_tlv_parse_sax_words(unsigned data_len, char *data, unsigned words_size, unsigned short *words);
int hp_tlv_parse_sax_dwords(unsigned data_len, char *data, unsigned dwords_size, unsigned long *dwords);
int hp_tlv_parse_sax_qwords(unsigned data_len, char *data, unsigned qwords_size, unsigned long long *qwords);

int hp_tlv_parse_dom(struct hp_tlv_stream *st, struct hp_tlv_node **nd);

#endif /* !defined(__HP_TLV_H) */
