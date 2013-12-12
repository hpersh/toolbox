
#include "log/hp_log.h"

void
hp_assert_fail(char * const fac, char * const func, unsigned line, char * const expr)
{
  hp_log(fac, HP_LOG_LVL_FATAL, func, line, "Assertion failed, function %s, line %u: %s", expr);
}

