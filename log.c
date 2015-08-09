/**
 * log.c
 */
#include "log.h"

void wm_log(const char* msg, LogLevel level) {
  if(level < LOG_LEVEL) {
      return;
  }

  // Get datetime string
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char datetime[256] = "";
  snprintf(datetime, 256, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
      tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  fprintf(stdout, "[wm] %-6s \u2022 %s | %s\n", log_level_string[level], datetime, msg);
}
