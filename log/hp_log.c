#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hp_log.h"
#include "common/hp_common.h"
#include "assert/hp_assert.h"


void
hp_log(char *mod, enum hp_log_lvl lvl, char *file, unsigned linenum, char *fmt, ...)
{
  static const char * const lvl_str_tbl[] = {
    "|DEBUG|",
    "|TRACE|",
    "|INFO |",
    "|WARN |",
    "|ERR  |",
    "|CRIT |",
    "|FATAL|"
  };
  
  char     buf[HP_LOG_MAX_LEN];
  va_list  ap;
  unsigned n;

  HP_ASSERT(lvl < ARRAY_SIZE(lvl_str_tbl));

  va_start(ap, fmt);

  hp_log_time_get(buf);
  n = strlen(buf);

  strcpy(buf + n, "|");
  ++n;

  if (mod) {
    strcpy(buf + n, mod);
    n = strlen(buf);
  }

  strcpy(buf + n, lvl_str_tbl[lvl]);
  n = strlen(buf);

  snprintf(buf + n, sizeof(buf) - n, "%s:%u|", file, linenum);
  n = strlen(buf);

  vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

  hp_log_puts(mod, lvl, buf);

  va_end(ap);
}

