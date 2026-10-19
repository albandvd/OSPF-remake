#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "log.h"

static char g_hostname[HOST_NAME_MAX + 1] = "?";
static int g_debug_enabled = 0;

void log_init(void)
{
    /* stdout is fully buffered when not attached to a TTY (the normal case
     * under Docker), so printf output can sit in libc's buffer for a long
     * time before `docker logs` shows it. Line-buffering makes every
     * newline-terminated message appear immediately. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (gethostname(g_hostname, sizeof(g_hostname)) != 0)
        strncpy(g_hostname, "?", sizeof(g_hostname) - 1);

    const char *level = getenv("LOG_LEVEL");
    g_debug_enabled = (level && strcmp(level, "debug") == 0);
}

static void log_timestamp(char *out, size_t out_len)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(out, out_len, "%H:%M:%S", &tm_now);
}

void log_event(const char *event, const char *fmt, ...)
{
    char ts[16];
    log_timestamp(ts, sizeof(ts));

    printf("[%s] [%s] %s", ts, g_hostname, event);
    if (fmt && fmt[0] != '\0')
    {
        printf(" ");
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    printf("\n");
}

void log_debug(const char *fmt, ...)
{
    if (!g_debug_enabled)
        return;

    char ts[16];
    log_timestamp(ts, sizeof(ts));

    printf("[%s] [%s] DEBUG ", ts, g_hostname);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
