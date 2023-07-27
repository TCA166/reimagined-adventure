#include <syslog.h>
//Header file for overriding printf functions with syslogs

#define printf(...) syslog(LOG_NOTICE, __VA_ARGS__)

#define fprintf(loc,...) syslog(LOG_ERR, __VA_ARGS__)
