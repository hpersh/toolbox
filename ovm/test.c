#include <stdio.h>
#include <string.h>

#include "ovm.h"

#define _ARRAY_SIZE(a)  (sizeof(a) / sizeof((a)[0]))


void
obj_set_new(struct ovm *vm)
{
  ovm_newc(vm, R0, OBJ_TYPE_DICT, 32);
}

void
obj_set_insert(struct ovm *vm)
{
  ovm_pushm(vm, R1, 3);

  ovm_pick(vm, R1, 3);
  ovm_pick(vm, R2, 4);
  ovm_new(vm, R3, OBJ_TYPE_NIL);

  ovm_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  ovm_popm(vm, R1, 3);
  
  ovm_dropn(vm, 2);
}

void
obj_set_del(struct ovm *vm)
{
  ovm_pushm(vm, R1, 2);

  ovm_pick(vm, R1, 2);
  ovm_pick(vm, R2, 3);

  ovm_call(vm, R1, OBJ_OP_DEL, R2);

  ovm_popm(vm, R1, 2);
  
  ovm_dropn(vm, 2);
}

void
obj_set_member(struct ovm *vm)
{
  ovm_pushm(vm, R1, 2);

  ovm_pick(vm, R0, 2);
  ovm_pick(vm, R1, 3);
  ovm_new(vm, R2, OBJ_TYPE_NIL);

  ovm_call(vm, R0, OBJ_OP_AT, R1);
  ovm_call(vm, R0, OBJ_OP_EQ, R2);
  ovm_call(vm, R0, OBJ_OP_NOT);

  ovm_popm(vm, R1, 2);
  
  ovm_dropn(vm, 2);
}

void
obj_set_members(struct ovm *vm)
{
  ovm_pop(vm, R0);
  ovm_call(vm, R0, OBJ_OP_KEYS);
}

void
obj_fprint(struct ovm *vm, FILE *fp)
{
  ovm_push(vm, R1);

  ovm_pick(vm, R1, 1);
  ovm_new(vm, R1, OBJ_TYPE_STRING, R1);
  fprintf(fp, "%s", ovm_string_val(vm, R1));

  ovm_pop(vm, R1);

  ovm_drop(vm);
}

void
obj_print(struct ovm *vm)
{
  obj_fprint(vm, stdout);
}

struct obj obj_pool[1000], *obj_stack[100];

struct {
  obj_var cmd_dict, cmd_func_dict;
} obj_work[1];

struct ovm vm[1];

void
show_env(struct ovm *vm)
{
  printf("Got show env command\n");
}

void
show_sys(struct ovm *vm)
{
  printf("Got show sys command\n");
}

void
set_date(struct ovm *vm)
{
  printf("Got set date command\n");

  ovm_newc(vm, R1, OBJ_TYPE_INTEGER, 2);
  ovm_call(vm, R0, OBJ_OP_AT, R1);
  ovm_new(vm, R2, OBJ_TYPE_INTEGER, R0);

  printf("Set date to %lld\n", ovm_integer_val(vm, R2));
}

void
set_time(struct ovm *vm)
{
  printf("Got set time  command\n");
}

void
test_ram_short(struct ovm *vm)
{
  printf("Got test ram short command\n");
}

void
test_ram_long(struct ovm *vm)
{
  printf("Got test ram long command\n");
}

void
test_clock(struct ovm *vm)
{
  printf("Got test clock command\n");
}


int
main(void)
{
  ovm_init(vm, sizeof(obj_pool), obj_pool, sizeof(obj_work), obj_work, sizeof(obj_stack), obj_stack);

#if 0
  ovm_init(vm, 1000, 1000);

  obj_set_new(vm);
  ovm_move(vm, R1, R0);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  ovm_push(vm, R2);
  ovm_push(vm, R1);
  obj_set_insert(vm);

  ovm_push(vm, R2);
  ovm_push(vm, R1);
  obj_set_member(vm);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "sam");
  ovm_push(vm, R2);
  ovm_push(vm, R1);
  obj_set_insert(vm);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  ovm_push(vm, R2);
  ovm_push(vm, R1);
  obj_set_insert(vm);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "bar");
  ovm_push(vm, R2);
  ovm_push(vm, R1);
  obj_set_insert(vm);

  ovm_push(vm, R1);
  obj_set_members(vm);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_push(vm, R1);
  obj_set_members(vm);
  ovm_newc(vm, R3, OBJ_TYPE_INTEGER, 1);
  ovm_newc(vm, R4, OBJ_TYPE_INTEGER, 1);
  ovm_call(vm, R0, OBJ_OP_SLICE, R3, R4);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");
#endif

#if 1
  ovm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 0);
  ovm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 10000000);
  ovm_newc(vm, R2, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1);
  for (;;) {
    ovm_move(vm, R3, R0);
    ovm_call(vm, R3, OBJ_OP_LT, R1);
    if (!ovm_bool_val(vm, R3))  break;
    ovm_call(vm, R0, OBJ_OP_ADD, R2);
  }
#endif

#if 0
  ovm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1234);
  ovm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 5678);
  ovm_call(vm, R0, OBJ_OP_ADD, R1);
  ovm_new(vm, R1, OBJ_TYPE_STRING, R0);

  ovm_new(vm, R4, OBJ_TYPE_PAIR, 2, R0, R1);
  ovm_move(vm, R5, R4);
  ovm_call(vm, R5, OBJ_OP_CAR);

  printf("%lld", obj_integer_val(vm, R5));
  printf("%s", obj_string_val(vm, R1));

  ovm_newc(vm, R0, OBJ_TYPE_INTEGER, (obj_integer_val_t) 0);
  ovm_newc(vm, R1, OBJ_TYPE_INTEGER, (obj_integer_val_t) 10);
  ovm_newc(vm, R2, OBJ_TYPE_INTEGER, (obj_integer_val_t) 1);
  for (;;) {
    ovm_move(vm, R3, R0);
    ovm_call(vm, R3, OBJ_OP_LT, R1);
    if (!obj_bool_val(vm, R3))  break;

    ovm_push(vm, R0);  obj_print(vm);  printf("\n");
    
    ovm_call(vm, R0, OBJ_OP_ADD, R2);
  }

  ovm_newc(vm, R1, OBJ_TYPE_DICT, (obj_integer_val_t) 32);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "sam");
  ovm_newc(vm, R3, OBJ_TYPE_FLOAT, (obj_float_val_t) 3.14);
  ovm_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  ovm_newc(vm, R3, OBJ_TYPE_INTEGER, (obj_integer_val_t) 42);
  ovm_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "bar");
  ovm_newc(vm, R3, OBJ_TYPE_STRING, "xxx");
  ovm_call(vm, R1, OBJ_OP_AT_PUT, R2, R3);
  
  ovm_push(vm, R1);  obj_print(vm);  printf("\n");

  ovm_move(vm, R4, R1);
  ovm_call(vm, R4, OBJ_OP_KEYS);

  ovm_push(vm, R4);  obj_print(vm);  printf("\n");

  ovm_move(vm, R2, R1);
  ovm_newc(vm, R3, OBJ_TYPE_STRING, "foo");
  ovm_call(vm, R2, OBJ_OP_AT, R3);

  ovm_push(vm, R2);  obj_print(vm);  printf("\n");

  ovm_move(vm, R2, R1);
  ovm_newc(vm, R3, OBJ_TYPE_STRING, "sam");
  ovm_call(vm, R2, OBJ_OP_AT, R3);

  ovm_push(vm, R2);  obj_print(vm);  printf("\n");

  ovm_move(vm, R2, R1);
  ovm_newc(vm, R3, OBJ_TYPE_STRING, "gorp");
  ovm_call(vm, R2, OBJ_OP_AT, R3);
  assert(ovm_type(vm, R2) == OBJ_TYPE_NIL);

  ovm_newc(vm, R2, OBJ_TYPE_STRING, "foo");
  ovm_call(vm, R1, OBJ_OP_DEL, R2);

  ovm_push(vm, R1);  obj_print(vm);  printf("\n");

  ovm_new(vm, R2, OBJ_TYPE_ARRAY, R1);

  ovm_push(vm, R2);  obj_print(vm);  printf("\n");

  ovm_newc(vm, R1, OBJ_TYPE_STRING, "The cat in the hat");
  ovm_newc(vm, R2, OBJ_TYPE_INTEGER, -5);
  ovm_newc(vm, R3, OBJ_TYPE_INTEGER, -5);
  ovm_move(vm, R4, R1);
  ovm_call(vm, R4, OBJ_OP_SLICE, R2, R3);

  ovm_push(vm, R4);  obj_print(vm);  printf("\n");
#endif
  
#if 0
  ovm_mode(vm, R4, R1);
  ovm_call(vm, R4, OBJ_OP_AT, R2);
  ovm_move(vm, R2, R1);
  ovm_call(vm, R2, OBJ_OP_CAR);

  ovm_push(vm, R2);  obj_print(vm);  printf("\n");

  ovm_move(vm, R2, R1);
  ovm_call(vm, R2, OBJ_OP_CDR);

  ovm_push(vm, R2);  obj_print(vm);  printf("\n");
#endif

#if 0
  {
    ovm_news(vm, R0, "{a: foo, b: 13}");

    ovm_push(vm, R0);  obj_print(vm);  printf("\n");

    ovm_move(vm, R1, R0);

    ovm_news(vm, R2, "b");

    ovm_call(vm, R1, OBJ_OP_AT, R2);

    ovm_push(vm, R1);  obj_print(vm);  printf("\n");
  }
#endif

#if 0
  ovm_newc(vm, R0, OBJ_TYPE_STRING, "Foo the bar");

  ovm_call(vm, R0, OBJ_OP_REVERSE);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_call(vm, R0, OBJ_OP_REVERSE);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_newc(vm, R1, OBJ_TYPE_STRING, " ");

  ovm_call(vm, R0, OBJ_OP_SPLIT, R1);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_newc(vm, R1, OBJ_TYPE_STRING, ":");
  ovm_call(vm, R1, OBJ_OP_JOIN, R0);

  ovm_push(vm, R1);  obj_print(vm);  printf("\n");

#endif

#if 0

  {
    static char s[] = "\"Now is the winter of our discontent made glorious summer by this son of York\"";

    ovm_news(vm, R0, sizeof(s) - 1, s);
  }
    
  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_newc(vm, R1, OBJ_TYPE_STRING, 1, " ");

  ovm_call(vm, R0, OBJ_OP_SPLIT, R1);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_new(vm, R0, OBJ_TYPE_ARRAY, R0);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_call(vm, R0, OBJ_OP_SORT);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  ovm_call(vm, R0, OBJ_OP_REVERSE);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

  {
    static char s[] = "(#true, #false, #true, #false, #false, #true)";

    ovm_news(vm, R1, sizeof(s) - 1, s);
  }

  ovm_push(vm, R1);  obj_print(vm);  printf("\n");

  ovm_call(vm, R0, OBJ_OP_FILTER, R1);

  ovm_push(vm, R0);  obj_print(vm);  printf("\n");

#endif

#if 0

  {
    static char cmd_dict[] = "{"
          "\"show\": {"
              "\"environment\": \"show-env-cmd\","
              "\"system\": \"show-sys-cmd\""
          "},"
          "\"set\": {"
              "\"date\": {"
                  "\"@int\": \"set-date-cmd\""
              "},"
              "\"time\": \"set-time-cmd\""
          "},"
          "\"test\": {"
              "\"ram\": {"
                  "\"short\": \"test-ram-short-cmd\","
                  "\"long\": \"test-ram-long-cmd\""
              "},"
              "\"clock\": \"test-clock-cmd\""
          "}"
      "}";

    static struct {
      char *label;
      void (*func)(struct ovm *);
    } cmd_func_dict[] = {
      { "show-env-cmd",       show_env },
      { "show-sys-cmd",       show_sys },
      { "set-date-cmd",       set_date },
      { "set-time-cmd",       set_time },
      { "test-ram-short-cmd", test_ram_short },
      { "test-ram-long-cmd",  test_ram_long },
      { "test-clock-cmd",     test_clock }
    };

    unsigned i;

    ovm_news(vm, R0, sizeof(cmd_dict) - 1, cmd_dict);
    ovm_store(vm, R0, obj_work->cmd_dict);

    ovm_newc(vm, R0, OBJ_TYPE_DICT, 0);
    for (i = 0; i < _ARRAY_SIZE(cmd_func_dict); ++i) {
      ovm_newc(vm, R1, OBJ_TYPE_STRING, strlen(cmd_func_dict[i].label), cmd_func_dict[i].label);
      ovm_newc(vm, R2, OBJ_TYPE_POINTER, cmd_func_dict[i].func);
      ovm_call(vm, R0, OBJ_OP_AT_PUT, R1, R2);
    }
    ovm_store(vm, R0, obj_work->cmd_func_dict);
  }
  
  for (;;) {
    char buf[128];

    if (fgets(buf, sizeof(buf) - 1, stdin) == 0)  break;

    ovm_newc(vm, R0, OBJ_TYPE_STRING, strlen(buf) - 1, buf);
    ovm_newc(vm, R1, OBJ_TYPE_STRING, 1, " ");
    ovm_call(vm, R0, OBJ_OP_SPLIT, R1);
    
    ovm_load(vm, R1, obj_work->cmd_dict);

    for (ovm_new(vm, R2, OBJ_TYPE_NIL); ovm_type(vm, R1) == OBJ_TYPE_DICT && ovm_type(vm, R0) != OBJ_TYPE_NIL; ovm_call(vm, R0, OBJ_OP_CDR)) {
      ovm_move(vm, R3, R0);
      ovm_call(vm, R3, OBJ_OP_CAR);

      if (ovm_string_size(vm, R3) < 2)  continue;

      if (ovm_string_val(vm, R3)[0] == '@')  break;
      
      ovm_new(vm, R4, OBJ_TYPE_INTEGER, R3);
      if (ovm_errno(vm) == OBJ_ERRNO_NONE) {
	ovm_newc(vm, R4, OBJ_TYPE_STRING, 4, "@int");
      } else {
	ovm_err_clr(vm);
	ovm_new(vm, R4, OBJ_TYPE_FLOAT, R3);
	if (ovm_errno(vm) == OBJ_ERRNO_NONE) {
	  ovm_newc(vm, R4, OBJ_TYPE_STRING, 4, "@float");
	} else {
	  ovm_err_clr(vm);
	  ovm_move(vm, R4, R3);
	}
      }

      ovm_move(vm, R5, R1);
      ovm_call(vm, R5, OBJ_OP_AT, R4);

      if (ovm_type(vm, R5) == OBJ_TYPE_NIL)  break;

      ovm_call(vm, R5, OBJ_OP_CDR);
      ovm_move(vm, R1, R5);

      ovm_new(vm, R4, OBJ_TYPE_NIL);
      ovm_new(vm, R3, OBJ_TYPE_LIST, 2, R3, R4);
      ovm_call(vm, R2, OBJ_OP_APPEND, R3);
    }

    if (ovm_type(vm, R0) != OBJ_TYPE_NIL || ovm_type(vm, R1) != OBJ_TYPE_STRING) {
      printf("Invalid command\n");
      continue;
    }
    
    ovm_load(vm, R3, obj_work->cmd_func_dict);
    ovm_call(vm, R3, OBJ_OP_AT, R1);
    if (ovm_type(vm, R3) == OBJ_TYPE_NIL) {
      printf("Internal error!\n");
      continue;
    }
    
    ovm_call(vm, R3, OBJ_OP_CDR);
    ovm_move(vm, R0, R2);
    (* ((void (*)(struct ovm *)) ovm_ptr_val(vm, R3)))(vm);
  }

#endif

  return (0);  
}

void
show_all_commands(void)
{
  
}
