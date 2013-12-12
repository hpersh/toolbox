#include "hp_except.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

struct unit_test unit_test;

void
foo(void)
{
  if (unit_test.foo.raise)  HP_EXCEPT_RAISE(unit_test.foo.code, unit_test.foo.arg);
}

void
bar(void)
{
  struct hp_except_frame fr[1];

  HP_EXCEPT_BEGIN(fr) {
    HP_EXCEPT_TRY_BEGIN {
      foo();
    } HP_EXCEPT_TRY_END;

    HP_EXCEPT_CATCH_BEGIN(unit_test.bar.catch) {
      printf("Got exception in %s, code = %d, arg = %p\n", __FUNCTION__, HP_EXCEPT_CODE, HP_EXCEPT_ARG);
      ++unit_test.bar.caught;
      unit_test.bar.caught_code = HP_EXCEPT_CODE;
      unit_test.bar.caught_arg  = HP_EXCEPT_ARG;
      if (unit_test.bar.reraise)  HP_EXCEPT_RERAISE;
    } HP_EXCEPT_CATCH_END;

    HP_EXCEPT_NONE_BEGIN {
      printf("No exception in %s\n", __FUNCTION__);
      ++unit_test.bar.none;
    } HP_EXCEPT_NONE_END;
  } HP_EXCEPT_END;
}

int
test(void)
{
  struct hp_except_frame fr[1];

  HP_EXCEPT_BEGIN(fr) {
    HP_EXCEPT_TRY_BEGIN {
      bar();
    } HP_EXCEPT_TRY_END;
	
    HP_EXCEPT_CATCH_BEGIN(unit_test.test.catch) {
      printf("Got exception in %s, code = %d, arg = %p\n", __FUNCTION__, HP_EXCEPT_CODE, HP_EXCEPT_ARG);
      ++unit_test.test.caught;
      unit_test.test.caught_code = HP_EXCEPT_CODE;
      unit_test.test.caught_arg  = HP_EXCEPT_ARG;
      if (unit_test.test.reraise)  HP_EXCEPT_RERAISE;
    } HP_EXCEPT_CATCH_END;

    HP_EXCEPT_NONE_BEGIN {
      printf("No exception in %s\n", __FUNCTION__);
      ++unit_test.test.none;
    } HP_EXCEPT_NONE_END;
  } HP_EXCEPT_END;
}

int
main(void)
{
  int test_code  = 42;
  void *test_arg = (void *) 0x1234;

  /* Test normal operation, no exceptions */

  test();

  assert(unit_test.frp_nil == 0);
  assert(unit_test.bar.caught == 0);
  assert(unit_test.bar.none == 1);
  assert(unit_test.test.caught == 0);
  assert(unit_test.test.none == 1);

  /* Test catching, but no raising */

  memset(&unit_test, 0, sizeof(unit_test));

  unit_test.bar.catch = 1;

  test();

  assert(unit_test.frp_nil == 0);
  assert(unit_test.bar.caught == 0);
  assert(unit_test.bar.none == 1);
  assert(unit_test.test.caught == 0);
  assert(unit_test.test.none == 1);

  /* Test direct catching */

  memset(&unit_test, 0, sizeof(unit_test));

  unit_test.foo.raise = 1;
  unit_test.foo.code  = ++test_code;
  unit_test.foo.arg   = ++test_arg;
  unit_test.bar.catch = 1;

  test();

  assert(unit_test.frp_nil == 0);
  assert(unit_test.bar.caught == 1);
  assert(unit_test.bar.caught_code == test_code);
  assert(unit_test.bar.caught_arg  == test_arg);
  assert(unit_test.bar.none == 0);
  assert(unit_test.test.caught == 0);
  assert(unit_test.test.none == 1);

  /* Test indirect catching */

  memset(&unit_test, 0, sizeof(unit_test));

  unit_test.foo.raise  = 1;
  unit_test.foo.code   = ++test_code;
  unit_test.foo.arg    = ++test_arg;
  unit_test.test.catch = 1;

  test();

  assert(unit_test.frp_nil == 0);
  assert(unit_test.bar.caught == 0);
  assert(unit_test.bar.none == 0);
  assert(unit_test.test.caught == 1);
  assert(unit_test.test.caught_code == test_code);
  assert(unit_test.test.caught_arg  == test_arg);
  assert(unit_test.test.none == 0);

  /* Test re-raise */

  memset(&unit_test, 0, sizeof(unit_test));

  unit_test.foo.raise   = 1;
  unit_test.foo.code    = ++test_code;
  unit_test.foo.arg     = ++test_arg;
  unit_test.bar.catch   = 1;
  unit_test.bar.reraise = 1;
  unit_test.test.catch  = 1;

  test();

  assert(unit_test.frp_nil == 0);
  assert(unit_test.bar.caught == 1);
  assert(unit_test.bar.caught_code == test_code);
  assert(unit_test.bar.caught_arg  == test_arg);
  assert(unit_test.bar.none == 0);
  assert(unit_test.test.caught == 1);
  assert(unit_test.test.caught_code == test_code);
  assert(unit_test.test.caught_arg  == test_arg);
  assert(unit_test.test.none == 0);

  /* Test raising without catching */

  memset(&unit_test, 0, sizeof(unit_test));

  unit_test.foo.raise   = 1;
  unit_test.foo.code    = ++test_code;
  unit_test.foo.arg     = ++test_arg;

  test();

  assert(unit_test.frp_nil == 1);
  assert(unit_test.bar.caught == 0);
  assert(unit_test.test.caught == 0);
}
