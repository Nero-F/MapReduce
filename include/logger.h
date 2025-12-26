#ifndef _F_LOGGER_H_
#define _F_LOGGER_H_

#include <stdio.h>
#include <stdbool.h>

#define ANSI_ESC "\x1b"
#define RED ANSI_ESC "[1;31m"
#define GREEN ANSI_ESC "[1;32m"
#define YELLOW ANSI_ESC "[1;33m"
#define BLUE ANSI_ESC "[1;34m"
#define MAGENTA ANSI_ESC "[1;35m"
#define WHITE ANSI_ESC "[4;1;37m"
#define RESET ANSI_ESC "[0m"

#define LOG_LVL                                                                \
    LVL(INFO, BLUE)                                                            \
    LVL(WARNING, YELLOW)                                                       \
    LVL(ERROR, RED)                                                            \
    LVL(FATAL, MAGENTA)                                                        \
    LVL(CRITICAL, MAGENTA)                                                     \
    LVL(DEBUG, GREEN)

typedef enum f_log_lvl_e {
#define LVL(lvl, ...) lvl,
    LOG_LVL
#undef LVL
} f_log_lvl;

typedef struct f_logger_s {
    f_log_lvl lvl;
    char *s_lvl;
    char *color;
} f_logger_t;

extern f_logger_t g_loggers[];

#ifndef ENABLE_DEBUG
    #define ENABLE_DEBUG false
#endif

#define F_LOG(lvl, format, ...)                                                \
    do {                                                                       \
        if (lvl == DEBUG && ENABLE_DEBUG == true)                              \
            logger(lvl, format " " WHITE "|%s:%d|" RESET "\n", ##__VA_ARGS__,  \
                __FILE__, __LINE__);                                           \
        else if (lvl != DEBUG) {                                               \
            logger(lvl, format "\n", ##__VA_ARGS__);                           \
        }                                                                      \
    } while (0)

void logger(f_log_lvl, const char *, ...);

#endif // !_F_LOGGER_H_
