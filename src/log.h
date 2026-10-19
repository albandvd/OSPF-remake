#ifndef LOG_H
#define LOG_H

/* Pedagogical event logger.
 *
 * Output format: "[HH:MM:SS] [hostname] EVENT message"
 * log_init() must be called once at process start (including after fork()
 * in the child) — it line-buffers stdout/stderr so `docker logs` shows
 * output immediately instead of only after the libc buffer fills, and
 * caches the hostname used as the log prefix.
 *
 * LOG_LEVEL=debug enables log_debug() output (e.g. per-IP scan attempts),
 * which is silent by default to avoid spamming the log with noise.
 */

void log_init(void);
void log_event(const char *event, const char *fmt, ...);
void log_debug(const char *fmt, ...);

#endif /* LOG_H */
