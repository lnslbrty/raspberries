#ifndef LOG_H
#define LOG_H 1

#include <errno.h>
#include <string.h>

#include "config.h"

#ifdef NO_DEBUG
#include <syslog.h>
#define log_init()          openlog(appname, LOG_NDELAY, LOG_DAEMON)
#define log_ret( prefix )   { if (errno != 0) syslog(LOG_ERR, "%s returned %d: %m", prefix, errno); }
#define log(fmt, ...)       syslog(LOG_INFO , fmt, __VA_ARGS__)
#define log_err(fmt, ...)   syslog(LOG_ERR  , fmt, __VA_ARGS__)
#define log_emerg(fmt, ...) syslog(LOG_EMERG, fmt, __VA_ARGS__)
#define log_close()         closelog();
#else
#define log_init()
#define log_ret( prefix )   { if (errno != 0) fprintf(stderr, "%s returned %d: %s\n", prefix, errno, strerror(errno)); fprintf(stderr, "\n"); }
#define log(fmt, ...)       fprintf(stderr, fmt, __VA_ARGS__); fprintf(stderr, "\n");
#define log_err(fmt, ...)   log(fmt, __VA_ARGS__)
#define log_emerg(fmt, ...) log(fmt, __VA_ARGS__)
#define log_close()
#endif

#endif
