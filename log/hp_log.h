
enum hp_log_lvl {
  HP_LOG_LVL_DEBUG,
  HP_LOG_LVL_TRACE,
  HP_LOG_LVL_INFO,
  HP_LOG_LVL_WARN,
  HP_LOG_LVL_ERR,
  HP_LOG_LVL_CRIT,
  HP_LOG_LVL_FATAL
};

enum {
  HP_LOG_MAX_LEN = 132
};

void hp_log(char *mod, enum hp_log_lvl lvl, char *file, unsigned line_num, char *fmt, ...);

#define HP_LOG(mod, lvl, ...)					\
  hp_log((mod), (lvl), (char *) __FILE__, __LINE__, __VA_ARGS__)

/***************************************************************************/

/* To be provided by platform */

/* Current time, in the form of asctime(): "Wed Jun 30 21:49:08 1993\n" */
extern void hp_log_time_get(char *buf);

/* Write given string to log output */
extern void hp_log_puts(char *mod, enum hp_log_lvl lvl, char *buf);
