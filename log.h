/**
 * log.h
 */
#ifndef LOG_H
#define LOG_H

#include <time.h>
#include <stdio.h>

#define LOG_LEVEL DEBUG

typedef enum {DEBUG,INFO,WARN,ERROR} LogLevel;

const static char* log_level_string[4] = {"DEBUG", "INFO", "WARN", "ERROR"};

void wm_log(const char*  msg, LogLevel level);

#endif
