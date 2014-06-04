#include <string.h>
#include <assert.h>

#include "hp_json.h"

static inline
int
hp_json_stream_getc(struct hp_json_stream *st)
{
  return (hp_stream_getc(st->iost));
}

static inline
int
hp_json_stream_ungetc(struct hp_json_stream *st, char c)
{
  return (hp_stream_ungetc(st->iost, c));
}

static inline
int
hp_json_stream_putc(struct hp_json_stream *st, char c)
{
  return (hp_stream_putc(st->iost, c));
}

static inline
int
hp_json_stream_puts(struct hp_json_stream *st, char *s)
{
  return (hp_stream_puts(st->iost, s));
}


enum hp_json_tostring_state {
  HP_JSON_TOSTRING_STATE_START,
  HP_JSON_TOSTRING_STATE_ARRAY,
  HP_JSON_TOSTRING_STATE_DICT_KEY,
  HP_JSON_TOSTRING_STATE_DICT_VAL
};

struct hp_json_stream *
hp_json_stream_tostring_init(struct hp_json_stream *st,
			     struct hp_stream      *iost
			     )
{
  st->iost     = iost;
  st->parent   = 0;
  st->state    = HP_JSON_TOSTRING_STATE_START;
  st->item_cnt = 0;

  return (st);
}

static int
hp_json_tostring_before(struct hp_json_stream *st)
{
  int result = 0;

  switch (st->state) {
  case HP_JSON_TOSTRING_STATE_ARRAY:
  case HP_JSON_TOSTRING_STATE_DICT_KEY:
    if (st->item_cnt > 0)  result = hp_json_stream_putc(st, ',');
    break;
  case HP_JSON_TOSTRING_STATE_DICT_VAL:
    result = hp_json_stream_putc(st, ':');
  default:
    ;
  }

  return (result);
}

static int
hp_json_tostring_after(struct hp_json_stream *st)
{
  switch (st->state) {
  case HP_JSON_TOSTRING_STATE_ARRAY:
    ++st->item_cnt;
    break;
  case HP_JSON_TOSTRING_STATE_DICT_KEY:
    st->state = HP_JSON_TOSTRING_STATE_DICT_VAL;
    break;
  case HP_JSON_TOSTRING_STATE_DICT_VAL:
    st->state = HP_JSON_TOSTRING_STATE_DICT_KEY;
    ++st->item_cnt;
  default:
    ;
  }

  return (0);
}

#define TRY(x)   do { if ((x) < 0)  return (-1); } while (0)
#define TRYN(x)  do { if ((n = (x)) < 0)  return (-1);   result += n; } while (0)

#define HP_JSON_NUM_TOSTRING(_bufsize, _fmt)			  \
  int  n, result = 0;						  \
  char buf[_bufsize];						  \
								  \
  TRYN(hp_json_tostring_before(st));				  \
  TRYN(snprintf(buf, sizeof(buf), _fmt, val));			  \
  if (n > sizeof(buf))  return (-1);				  \
  TRYN(hp_json_stream_puts(st, buf));				  \
  TRYN(hp_json_tostring_after(st));				  \
  return (result);


int
hp_json_int_tostring(struct hp_json_stream *st, int val)
{
  HP_JSON_NUM_TOSTRING(16, "%d");
}

int
hp_json_hex_tostring(struct hp_json_stream *st, int val)
{
  HP_JSON_NUM_TOSTRING(16, "0x%x");
}

int
hp_json_float_tostring(struct hp_json_stream *st, double val)
{
  HP_JSON_NUM_TOSTRING(32, "%g");
}

int
hp_json_string_tostring(struct hp_json_stream *st, char *s)
{
  int  n, result = 0;
  char c;

  TRYN(hp_json_tostring_before(st));

  TRYN(hp_json_stream_putc(st, '"'));
  for ( ; c = *s; ++s) {
    if (c == '"')  TRYN(hp_json_stream_putc(st, '\\'));
    TRYN(hp_json_stream_putc(st, c));
  }
  TRYN(hp_json_stream_putc(st, '"'));

  TRYN(hp_json_tostring_after(st));

  return (result);
}

int
hp_json_arr_begin_tostring(struct hp_json_stream *st, struct hp_json_stream *ast)
{
  int n, result = 0;

  TRYN(hp_json_tostring_before(st));

  ast->iost     = st->iost;
  ast->parent   = st;
  ast->state    = HP_JSON_TOSTRING_STATE_ARRAY;
  ast->item_cnt = 0;

  TRYN(hp_json_stream_putc(st, '['));

  return (result);
}

int
hp_json_arr_end_tostring(struct hp_json_stream *ast)
{
  int n, result = 0;

  assert(ast->state == HP_JSON_TOSTRING_STATE_ARRAY);

  TRYN(hp_json_stream_putc(ast, ']'));
  TRYN(hp_json_tostring_after(ast->parent));

  return (result);
}

int
hp_json_dict_begin_tostring(struct hp_json_stream *st, struct hp_json_stream *dst)
{
  int n, result = 0;

  TRYN(hp_json_tostring_before(st));

  dst->iost     = st->iost;
  dst->parent   = st;
  dst->state    = HP_JSON_TOSTRING_STATE_DICT_KEY;
  dst->item_cnt = 0;

  TRYN(hp_json_stream_putc(st, '{'));

  return (result);
}

int
hp_json_dict_end_tostring(struct hp_json_stream *dst)
{
  int n, result = 0;

  assert(dst->state == HP_JSON_TOSTRING_STATE_DICT_KEY);

  TRYN(hp_json_stream_putc(dst, '}'));
  TRYN(hp_json_tostring_after(dst->parent));

  return (result);
}


enum hp_json_parse_state {
  HP_JSON_PARSE_STATE_OBJ,
  HP_JSON_PARSE_STATE_ARRAY_OBJ,
  HP_JSON_PARSE_STATE_ARRAY_COMMA,
  HP_JSON_PARSE_STATE_DICT_KEY,
  HP_JSON_PARSE_STATE_DICT_COLON,
  HP_JSON_PARSE_STATE_DICT_VAL,
  HP_JSON_PARSE_STATE_DICT_COMMA
};

enum hp_json_parse_tok {
  HP_JSON_PARSE_TOK_EOF,
  HP_JSON_PARSE_TOK_INT,
  HP_JSON_PARSE_TOK_HEX,
  HP_JSON_PARSE_TOK_FLOAT,
  HP_JSON_PARSE_TOK_STRING,
  HP_JSON_PARSE_TOK_LSQBR,
  HP_JSON_PARSE_TOK_COMMA,
  HP_JSON_PARSE_TOK_RSQBR,
  HP_JSON_PARSE_TOK_LBR,
  HP_JSON_PARSE_TOK_COLON,
  HP_JSON_PARSE_TOK_RBR
};

static int
hp_json_parse_tok(struct hp_json_stream *st, unsigned *tok, char *tokbuf, unsigned tokbufsize)
{
  char     c;
  unsigned result = 0;
  
  do {
    c = hp_json_stream_getc(st);
    if (c == -1) {
      *tok = HP_JSON_PARSE_TOK_EOF;
      return (result);
    }
    ++result;
  } while (isspace(c));

  switch (c) {
  case '[':
    *tok = HP_JSON_PARSE_TOK_LSQBR;
    return (result);

  case ',':
    *tok = HP_JSON_PARSE_TOK_COMMA;
    return (result);

  case ']':
    *tok = HP_JSON_PARSE_TOK_RSQBR;
    return (result);

  case '{':
    *tok = HP_JSON_PARSE_TOK_LBR;
    return (result);

  case ':':
    *tok = HP_JSON_PARSE_TOK_COLON;
    return (result);

  case '}':
    *tok = HP_JSON_PARSE_TOK_RBR;
    return (result);

  case '"':
    assert(tokbufsize > 0);
    
    for (*tok = HP_JSON_PARSE_TOK_STRING, --tokbufsize; tokbufsize; ++tokbuf, --tokbufsize) {
      c = hp_json_stream_getc(st);
      ++result;
      if (c == -1)  return (-1);
      switch (c) {
      case '\\':
	c = hp_json_stream_getc(st);
	++result;
	if (c == -1)  return (-1);
	break;
      case '"':
	*tokbuf = 0;
	return (result);
      default:
	;
      }

      *tokbuf = c;
    }
  }    

  if (c == '+' || c == '-' || c >= '0' && c <= '9') {
    unsigned n, ecnt;
    
    assert(tokbufsize > 0);

    *tok = HP_JSON_PARSE_TOK_INT;

    for (--tokbufsize, n = 0;;) {
      if (tokbufsize == 0)  return (-1);
      *tokbuf++ = c;
      --tokbufsize;
      ++n;
      
      c = hp_json_stream_getc(st);
      if (c == -1) {
      done:
	switch (c = tokbuf[-1]) {
	case '+':
	case '-':
	  return (-1);
	}
	if ((c | 0x20) == 'e')  return (-1);

	*tokbuf = 0;
	return (result);
      }
      ++result;
      
      switch (*tok) {
      case  HP_JSON_PARSE_TOK_INT:
	if (n == 1 && (c | 0x20) == 'x' || tokbuf[-1] == '0') {
	  *tok = HP_JSON_PARSE_TOK_HEX;
	  break;
	}
	
	if (c == '.') {
	  *tok = HP_JSON_PARSE_TOK_FLOAT;
	  ecnt = 0;
	  break;
	}
	
	if (c >= '0' && c <= '9')  break;

	hp_json_stream_ungetc(st, c);
	--result;
	goto done;

      case HP_JSON_PARSE_TOK_HEX:
	
	if (c >= '0' && c <= '9' || (c |= 0x20) >= 'a' && c <= 'f')  break;

	hp_json_stream_ungetc(st, c);
	--result;
	goto done;

      case HP_JSON_PARSE_TOK_FLOAT:
	if (c >= '0' && c <= '9')  break;
	if ((c | 0x20) == 'e') {
	  if (++ecnt == 1)  break;
	  return (-1);
	}
	if (c == '+' || c == '-') {
	  if ((tokbuf[-1] | 0x20) == 'e')  break;
	  return (-1);
	}

	hp_json_stream_ungetc(st, c);
	--result;
	goto done;
      }
    }
  }

  return (-1);
}

struct hp_json_stream *
hp_json_stream_parse_init(struct hp_json_stream *st,
			  struct hp_stream      *iost
			  )
{
  st->iost     = iost;
  st->parent   = 0;
  st->state    = HP_JSON_PARSE_STATE_OBJ;
  st->item_cnt = 0;

  return (st);
}

int
hp_json_parse(struct hp_json_stream   *st,
	      struct hp_json_parseval *pval,
	      struct hp_json_stream   *cst
	      )
{
  struct hp_json_stream *ust = 0;
  unsigned              tok;
  char                  tokbuf[64];
  int                   n, result = 0;

 again:
  TRYN(hp_json_parse_tok(st, &tok, tokbuf, sizeof(tokbuf)));

  switch (tok) {
  case HP_JSON_PARSE_TOK_EOF:
    if (st->state != HP_JSON_PARSE_STATE_OBJ)  return (-1);

    pval->code = HP_JSON_PARSE_EOF;
    return (result);

  case HP_JSON_PARSE_TOK_LSQBR:
    cst->parent   = st;
    cst->iost       = st->iost;
    cst->state    = HP_JSON_PARSE_STATE_ARRAY_OBJ;
    cst->item_cnt = 0;

    pval->code = HP_JSON_PARSE_ARRAY_BEGIN;
    return (result);
    
  case HP_JSON_PARSE_TOK_COMMA:
    switch (st->state) {
    case HP_JSON_PARSE_STATE_ARRAY_COMMA:
      st->state = HP_JSON_PARSE_STATE_ARRAY_OBJ;
      goto again;
    case HP_JSON_PARSE_STATE_DICT_COMMA:
      st->state = HP_JSON_PARSE_STATE_DICT_KEY;
      goto again;
    }

    return (-1);

  case HP_JSON_PARSE_TOK_RSQBR:
    switch (st->state) {
    case HP_JSON_PARSE_STATE_ARRAY_OBJ:
      if (st->item_cnt != 0)  return (-1);
    case HP_JSON_PARSE_STATE_ARRAY_COMMA:
      break;
    default:
      return (-1);
    }

    pval->code = HP_JSON_PARSE_ARRAY_END;
    ust        = st->parent;

    break;
    
  case HP_JSON_PARSE_TOK_LBR:
    cst->parent   = st;
    cst->iost       = st->iost;
    cst->state    = HP_JSON_PARSE_STATE_DICT_KEY;
    cst->item_cnt = 0;

    pval->code = HP_JSON_PARSE_DICT_BEGIN;
    return (result);

  case HP_JSON_PARSE_TOK_COLON:
    if (st->state == HP_JSON_PARSE_STATE_DICT_COLON) {
      st->state = HP_JSON_PARSE_STATE_DICT_VAL;
      goto again;
    }

    return (-1);
    
  case HP_JSON_PARSE_TOK_RBR:
    switch (st->state) {
    case HP_JSON_PARSE_STATE_DICT_KEY:
      if (st->item_cnt != 0)  return (-1);
    case HP_JSON_PARSE_STATE_DICT_COMMA:
      break;
    default:
      return (-1);
    }

    pval->code = HP_JSON_PARSE_DICT_END;
    ust        = st->parent;

    break;
    
  case HP_JSON_PARSE_TOK_INT:
    sscanf(tokbuf, "%d", &pval->intval);

    pval->code = HP_JSON_PARSE_INT;
    ust        = st;

    break;

  case HP_JSON_PARSE_TOK_HEX:
    sscanf(tokbuf + 2, "%x", &pval->intval);

    pval->code = HP_JSON_PARSE_INT;
    ust        = st;

    break;

  case HP_JSON_PARSE_TOK_FLOAT:
    sscanf(tokbuf, "%lg", &pval->floatval);

    pval->code = HP_JSON_PARSE_FLOAT;
    ust        = st;

    break;

  case HP_JSON_PARSE_TOK_STRING:
    if (pval->strbufsize <= n)  return (-1);
    strcpy(pval->strbuf, tokbuf);

    pval->code = HP_JSON_PARSE_STRING;
    ust        = st;

    break;

  default:
    return (-1);
  }

  switch (ust->state) {
  case HP_JSON_PARSE_STATE_ARRAY_OBJ:
    ++ust->item_cnt;
    ust->state = HP_JSON_PARSE_STATE_ARRAY_COMMA;
    break;
  case HP_JSON_PARSE_STATE_DICT_KEY:
    ust->state = HP_JSON_PARSE_STATE_DICT_COLON;
    break;
  case HP_JSON_PARSE_STATE_DICT_VAL:
    ++ust->item_cnt;
    ust->state = HP_JSON_PARSE_STATE_DICT_COMMA;
    break;
  default:
    ;
  }

  return (result);
}
