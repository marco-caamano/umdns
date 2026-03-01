#ifndef UMDNS_LOG_H
#define UMDNS_LOG_H

#include <stdbool.h>

typedef enum umdns_log_level {
    UMDNS_LOG_DEBUG = 0,
    UMDNS_LOG_INFO,
    UMDNS_LOG_WARN,
    UMDNS_LOG_ERROR
} umdns_log_level_t;

int umdns_log_init(umdns_log_level_t level, const char *file_path);
void umdns_log_close(void);
void umdns_log_set_level(umdns_log_level_t level);
umdns_log_level_t umdns_log_level_from_string(const char *text, bool *ok);

void umdns_log_debug(const char *fmt, ...);
void umdns_log_info(const char *fmt, ...);
void umdns_log_warn(const char *fmt, ...);
void umdns_log_error(const char *fmt, ...);

#endif
