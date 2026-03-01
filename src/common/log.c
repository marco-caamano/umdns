#include "umdns/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static FILE *g_log_stream = NULL;
static umdns_log_level_t g_log_level = UMDNS_LOG_INFO;

static const char *umdns_log_level_text(umdns_log_level_t level) {
    switch (level) {
        case UMDNS_LOG_DEBUG:
            return "DEBUG";
        case UMDNS_LOG_INFO:
            return "INFO";
        case UMDNS_LOG_WARN:
            return "WARN";
        case UMDNS_LOG_ERROR:
            return "ERROR";
        default:
            return "INFO";
    }
}

int umdns_log_init(umdns_log_level_t level, const char *file_path) {
    g_log_level = level;
    if (file_path == NULL || file_path[0] == '\0') {
        g_log_stream = stderr;
        return 0;
    }

    g_log_stream = fopen(file_path, "a");
    if (g_log_stream == NULL) {
        return -1;
    }
    return 0;
}

void umdns_log_close(void) {
    if (g_log_stream != NULL && g_log_stream != stderr) {
        fclose(g_log_stream);
    }
    g_log_stream = NULL;
}

void umdns_log_set_level(umdns_log_level_t level) {
    g_log_level = level;
}

umdns_log_level_t umdns_log_level_from_string(const char *text, bool *ok) {
    if (ok != NULL) {
        *ok = true;
    }

    if (text == NULL) {
        if (ok != NULL) {
            *ok = false;
        }
        return UMDNS_LOG_INFO;
    }

    if (strcasecmp(text, "debug") == 0) {
        return UMDNS_LOG_DEBUG;
    }
    if (strcasecmp(text, "info") == 0) {
        return UMDNS_LOG_INFO;
    }
    if (strcasecmp(text, "warn") == 0 || strcasecmp(text, "warning") == 0) {
        return UMDNS_LOG_WARN;
    }
    if (strcasecmp(text, "error") == 0) {
        return UMDNS_LOG_ERROR;
    }

    if (ok != NULL) {
        *ok = false;
    }
    return UMDNS_LOG_INFO;
}

static void umdns_log_write(umdns_log_level_t level, const char *fmt, va_list args) {
    time_t now = time(NULL);
    struct tm tm_now;
    char time_buffer[32];
    FILE *stream = g_log_stream == NULL ? stderr : g_log_stream;

    if (level < g_log_level) {
        return;
    }

    if (localtime_r(&now, &tm_now) == NULL) {
        snprintf(time_buffer, sizeof(time_buffer), "%s", "0000-00-00T00:00:00");
    } else {
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%dT%H:%M:%S", &tm_now);
    }

    fprintf(stream, "%s [%s] ", time_buffer, umdns_log_level_text(level));
    vfprintf(stream, fmt, args);
    fputc('\n', stream);
    fflush(stream);
}

void umdns_log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    umdns_log_write(UMDNS_LOG_DEBUG, fmt, args);
    va_end(args);
}

void umdns_log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    umdns_log_write(UMDNS_LOG_INFO, fmt, args);
    va_end(args);
}

void umdns_log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    umdns_log_write(UMDNS_LOG_WARN, fmt, args);
    va_end(args);
}

void umdns_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    umdns_log_write(UMDNS_LOG_ERROR, fmt, args);
    va_end(args);
}
