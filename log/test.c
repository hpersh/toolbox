#include "hp_log.h"

char fac[] = "Test";

int
main(void)
{
  HP_LOG(fac, HP_LOG_LVL_DEBUG, "This is a debug message: %d", 1234);
  
  HP_LOG(fac, HP_LOG_LVL_CRIT, "OMG!");

  HP_LOG(fac, HP_LOG_LVL_FATAL, "PANIC!!!");

  return (0);
}
