#include "logger.h"
#include <stdarg.h>

f_logger_t g_loggers[] = {
#define LVL(_lvl, _color)                                                      \
    (f_logger_t) {                                                             \
        .lvl = _lvl,                                                           \
        .s_lvl = #_lvl,                                                        \
        .color = _color,                                                       \
    },
    LOG_LVL
#undef LVL
};

void logger(f_log_lvl lvl, const char *format, ...)
{
    f_logger_t log = g_loggers[lvl];
    FILE *log_output = stdout;
    va_list ap;

    if (lvl == ERROR || lvl == CRITICAL || lvl == WARNING) log_output = stderr;

    va_start(ap, format);
    fprintf(log_output, "%s[%s]" RESET ": ", log.color, log.s_lvl);
    vfprintf(log_output, format, ap);
    va_end(ap);
}
