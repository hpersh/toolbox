#include <assert.h>

#include "hp_except.h"
#include "assert/hp_assert.h"

#ifdef __UNIT_TEST__
struct unit_test unit_test;
#endif

void
hp_except_raise(int code, void *arg)
{
#ifdef __UNIT_TEST__
  if (hp_except_frp == 0) {
    ++unit_test.frp_nil;

    return;
  }
#else
  HP_ASSERT(hp_except_frp != 0);
#endif

  HP_EXCEPT_CODE = code;
  HP_EXCEPT_ARG  = arg;
  longjmp(* (jmp_buf *) &hp_except_frp->jmp_buf, 1);
}
