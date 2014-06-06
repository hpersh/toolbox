#include "stream/hp_stream.h"

struct hp_json_stream {
  struct hp_stream      *iost;
  struct hp_json_stream *parent;
  unsigned              state, item_cnt;
};

struct hp_json_stream *hp_json_stream_tostring_init(struct hp_json_stream *st,
						    struct hp_stream      *iost
						    );

int hp_json_int_tostring(struct hp_json_stream *st, int val);
int hp_json_hex_tostring(struct hp_json_stream *st, int val);
int hp_json_float_tostring(struct hp_json_stream *st, double val);
int hp_json_string_tostring(struct hp_json_stream *st, char *s);
int hp_json_arr_begin_tostring(struct hp_json_stream *st, struct hp_json_stream *ast);
int hp_json_arr_end_tostring(struct hp_json_stream *ast);
int hp_json_dict_begin_tostring(struct hp_json_stream *st, struct hp_json_stream *dst);
int hp_json_dict_end_tostring(struct hp_json_stream *dst);

struct hp_json_stream *hp_json_stream_parse_init(struct hp_json_stream *st,
						 struct hp_stream      *iost
						 );

enum {
  HP_JSON_PARSE_EOF,
  HP_JSON_PARSE_INT,
  HP_JSON_PARSE_FLOAT,
  HP_JSON_PARSE_STRING,
  HP_JSON_PARSE_ARRAY_BEGIN,
  HP_JSON_PARSE_ARRAY_END,
  HP_JSON_PARSE_DICT_BEGIN,
  HP_JSON_PARSE_DICT_END
};

struct hp_json_parseval {
  unsigned code;
  int      intval;
  double   floatval;
  char     *strbuf;
  unsigned strbufsize;
};

int
hp_json_parse(struct hp_json_stream   *st,
	      struct hp_json_parseval *pval,
	      struct hp_json_stream   *cst
	      );


#define ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))
