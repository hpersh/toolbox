#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hp_log.h"
#include "common/hp_common.h"
#include "assert/hp_assert.h"

extern void hp_log_str(char *str); /* Must be supplied */
extern void hp_fatal(void);	/* Must be supplied */

void
hp_log(char *fac, enum hp_log_lvl lvl, char *func, unsigned linenum, char *fmt, ...)
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
  n = strlen(buf);

  if (fac) {
    strcpy(buf + n, fac);
    n = strlen(buf);
  }

  strcpy(buf + n, lvl_str_tbl[lvl]);
  n = strlen(buf);

  snprintf(buf + n, sizeof(buf) - n, "%s:%u|", func, linenum);
  n = strlen(buf);

  vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);

  hp_log_puts(fac, lvl, buf);

  va_end(ap);

  if (lvl == HP_LOG_LVL_FATAL)  hp_fatal();
}

