#include "hp_log.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

void
hp_log_time_get(char *s)
{
  time_t t[1];

  strcpy(s, ctime(t));
  s[strlen(s) - 1] = 0;
}

void
hp_log_puts(char *fac, enum hp_log_lvl lvl, char *s)
{
  puts(s);
}

void
hp_fatal(void)
{
  printf("Got FATAL indication\n");
}
