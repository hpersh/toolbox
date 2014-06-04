#include <string.h>
#include <assert.h>

#include "hp_json.h"




struct test {
  int   i;
  float f;
  int   a[5];
  char  *s;
};

int
test_json_dump(struct hp_json_stream *st, struct test *t)
{
  struct hp_json_stream dst[1];
  
  hp_json_dict_begin_tostring(st, dst);

  hp_json_string_tostring(dst, "i");
  hp_json_int_tostring(dst, t->i);

  hp_json_string_tostring(dst, "f");
  hp_json_float_tostring(dst, t->f);

  hp_json_string_tostring(dst, "a");
  {
    struct hp_json_stream ast[1];
    unsigned              i;
    
    hp_json_arr_begin_tostring(dst, ast);
    
    for (i = 0; i < ARRAY_SIZE(t->a); ++i)  hp_json_int_tostring(ast, t->a[i]);
    
    hp_json_arr_end_tostring(ast);
  }
  
  hp_json_string_tostring(dst, "s");
  hp_json_string_tostring(dst, t->s);
  
  hp_json_dict_end_tostring(dst);

  return (0);
}

int
test_json_parse(struct hp_json_stream *st, struct test *t)
{
  int                     n, result = 0;
  struct hp_json_parseval pval[1];
  char                    strbuf[64];
  struct hp_json_stream   dst[1], ast[1];
  
  pval->strbuf = strbuf;
  pval->strbufsize = sizeof(strbuf);

  n = hp_json_parse(st, pval, dst);
  assert(n > 0);
  result += n;
  assert(pval->code == HP_JSON_PARSE_DICT_BEGIN);
  
  for (;;) {
    n = hp_json_parse(dst, pval, 0);
    assert(n > 0);
    result += n;
    
    if (pval->code == HP_JSON_PARSE_DICT_END)  break;
    
    assert(pval->code == HP_JSON_PARSE_STRING);
    
    if (strcmp(pval->strbuf, "i") == 0) {
      n = hp_json_parse(dst, pval, 0);
      assert(n > 0);
      result += n;
      assert(pval->code == HP_JSON_PARSE_INT);
  
      t->i = pval->intval;

      continue;
    }
    if (strcmp(pval->strbuf, "f") == 0) {
      n = hp_json_parse(dst, pval, 0);
      assert(n > 0);
      result += n;
      assert(pval->code == HP_JSON_PARSE_FLOAT);
  
      t->f = pval->floatval;
    
      continue;
    }
    if (strcmp(pval->strbuf, "a") == 0) {
      struct hp_json_stream  ast[1];
      unsigned               i;

      n = hp_json_parse(dst, pval, ast);
      assert(n > 0);
      result += n;
      assert(pval->code == HP_JSON_PARSE_ARRAY_BEGIN);
  
      for (i = 0; i < ARRAY_SIZE(t->a); ++i) {
	n = hp_json_parse(ast, pval, 0);
	assert(n > 0);
	result += n;
	assert(pval->code == HP_JSON_PARSE_INT);
  
	t->a[i] = pval->intval;
      }

      n = hp_json_parse(ast, pval, 0);
      assert(n > 0);
      result += n;
      assert(pval->code == HP_JSON_PARSE_ARRAY_END);
      
      continue;
    }
    if (strcmp(pval->strbuf, "s") == 0) {
      n = hp_json_parse(dst, pval, 0);
      assert(n > 0);
      result += n;
      assert(pval->code == HP_JSON_PARSE_STRING);

      t->s = strdup(pval->strbuf);

      continue;
    }
    
    assert(0);
  }

  n = hp_json_parse(st, pval, 0);
  assert(n >= 0);
  result += n;
  assert(pval->code == HP_JSON_PARSE_EOF);

  return (result);
}


struct test test_out[1] = { {
    42, 3.14, { 2, 3, 5, 7, 11 }, "sam"
  } },
  test_in[1];

int
main(void)
{
  FILE                  *fp;
  struct hp_stream_file iost[1];
  struct hp_json_stream st[1];
  int                   n;

  fp = fopen("test.json", "w");
  assert(fp != 0);
  hp_stream_file_init(iost, fp);

  hp_json_stream_tostring_init(st, iost->base);

  test_json_dump(st, test_out);

  fclose(fp);

  fp = fopen("test.json", "r");
  assert(fp != 0);
  hp_stream_file_init(iost, fp);

  hp_json_stream_parse_init(st, iost->base);

  n = test_json_parse(st, test_in);

  assert(n > 0);

  assert(test_in->i == test_out->i);
  assert(test_in->f == test_out->f);
  assert(memcmp(test_in->a, test_out->a, sizeof(test_in->a)) == 0);
  assert(strcmp(test_in->s, test_out->s) == 0);

  return (0);
}
