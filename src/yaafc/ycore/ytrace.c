/* ytrace.c — switchable trace points. POSIX-only (pthread). */

#define _POSIX_C_SOURCE 200809L

#include <yaafc/ycore/ytrace.h>

#if YTRACE_C_ENABLED

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static ytrace_point_t g_points[YTRACE_C_MAX_POINTS];
static size_t g_point_count = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_initialized = false;
static bool g_default_enabled = false;

static struct ytime_timer *g_timers[YTIME_MAX_TIMERS];
static size_t g_timer_count = 0;

#define YTRACE_LOCK()   pthread_mutex_lock(&g_mutex)
#define YTRACE_UNLOCK() pthread_mutex_unlock(&g_mutex)

static bool stderr_supports_color(void)
{
    if (getenv("NO_COLOR")) return false;
    if (!isatty(2)) return false;
    const char *term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) return false;
    return true;
}

static void format_timestamp(char *buf, size_t bufsize)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    localtime_r(&ts.tv_sec, &tmv);
    int ms = (int)(ts.tv_nsec / 1000000);
    snprintf(buf, bufsize, "%02d:%02d:%02d.%03d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms);
}

#include <yaafc/yplatform/time.h>

double yaafc_ytime_monotonic_sec(void)
{
    return yaafc_yplatform_time_monotonic_sec();
}

#define ANSI_RESET  "\033[0m"
#define ANSI_GRAY   "\033[90m"
#define ANSI_CYAN   "\033[36m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED    "\033[31m"
#define ANSI_BOLD   "\033[1m"

static bool g_use_colors = false;

static void check_color_support(void)
{
    g_use_colors = stderr_supports_color();
}

static const char *level_color(const char *level)
{
    if (!g_use_colors) return "";
    if (strcmp(level, "trace") == 0) return ANSI_GRAY;
    if (strcmp(level, "debug") == 0) return ANSI_CYAN;
    if (strcmp(level, "info")  == 0) return ANSI_GREEN;
    if (strcmp(level, "warn")  == 0) return ANSI_YELLOW;
    if (strcmp(level, "error") == 0) return ANSI_RED ANSI_BOLD;
    return "";
}

static const char *color_reset(void)
{
    return g_use_colors ? ANSI_RESET : "";
}

void ytrace_init(void)
{
    YTRACE_LOCK();
    if (g_initialized) { YTRACE_UNLOCK(); return; }
    const char *def = getenv("YTRACE_DEFAULT_ON");
    if (def) {
        g_default_enabled = (strcmp(def, "yes") == 0 || strcmp(def, "1") == 0 ||
                             strcmp(def, "true") == 0);
    }
    check_color_support();
    g_initialized = true;
    YTRACE_UNLOCK();
}

void ytrace_shutdown(void)
{
    YTRACE_LOCK();
    g_point_count = 0;
    g_initialized = false;
    YTRACE_UNLOCK();
}

bool ytrace_register(bool *enabled, const char *file, int line, const char *func,
                     const char *level, const char *message)
{
    YTRACE_LOCK();
    if (!g_initialized) {
        YTRACE_UNLOCK();
        ytrace_init();
        YTRACE_LOCK();
    }
    *enabled = g_default_enabled;
    if (g_point_count < YTRACE_C_MAX_POINTS) {
        g_points[g_point_count] = (ytrace_point_t){.enabled = enabled,
                                                   .file = file,
                                                   .line = line,
                                                   .function = func,
                                                   .level = level,
                                                   .message = message};
        g_point_count++;
    } else {
        static bool warned = false;
        if (!warned) {
            fprintf(stderr, "[ytrace] WARNING: max trace points (%d) exceeded\n",
                    YTRACE_C_MAX_POINTS);
            warned = true;
        }
    }
    YTRACE_UNLOCK();
    return *enabled;
}

void ytrace_output(const char *level, const char *file, int line, const char *func,
                   const char *fmt, ...)
{
    char msg_buf[1024];
    char time_buf[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    format_timestamp(time_buf, sizeof(time_buf));

    const char *bname = strrchr(file, '/');
    if (!bname) bname = strrchr(file, '\\');
    if (bname) bname++; else bname = file;

    fprintf(stderr, "[%s] %s[%-5s]%s %s:%d (%s): %s\n", time_buf, level_color(level), level,
            color_reset(), bname, line, func, msg_buf);
}

void ytrace_set_all_enabled(bool enabled)
{
    YTRACE_LOCK();
    for (size_t i = 0; i < g_point_count; i++) *g_points[i].enabled = enabled;
    YTRACE_UNLOCK();
}

void ytrace_set_level_enabled(const char *level, bool enabled)
{
    YTRACE_LOCK();
    for (size_t i = 0; i < g_point_count; i++) {
        if (strcmp(g_points[i].level, level) == 0) *g_points[i].enabled = enabled;
    }
    YTRACE_UNLOCK();
}

void ytrace_set_file_enabled(const char *file, bool enabled)
{
    YTRACE_LOCK();
    for (size_t i = 0; i < g_point_count; i++) {
        const char *pb = strrchr(g_points[i].file, '/');
        if (!pb) pb = strrchr(g_points[i].file, '\\');
        pb = pb ? pb + 1 : g_points[i].file;
        if (strcmp(g_points[i].file, file) == 0 || strcmp(pb, file) == 0) {
            *g_points[i].enabled = enabled;
        }
    }
    YTRACE_UNLOCK();
}

void ytrace_set_function_enabled(const char *function, bool enabled)
{
    YTRACE_LOCK();
    for (size_t i = 0; i < g_point_count; i++) {
        if (strcmp(g_points[i].function, function) == 0) *g_points[i].enabled = enabled;
    }
    YTRACE_UNLOCK();
}

size_t ytrace_get_point_count(void)
{
    YTRACE_LOCK();
    size_t count = g_point_count;
    YTRACE_UNLOCK();
    return count;
}

const ytrace_point_t *ytrace_get_points(void) { return g_points; }

void ytrace_list(void)
{
    YTRACE_LOCK();
    fprintf(stderr, "\n[ytrace] Registered trace points: %zu\n", g_point_count);
    fprintf(stderr, "%-4s %-7s %-6s %-30s %-20s %s\n", "IDX", "ENABLED", "LEVEL", "FILE:LINE",
            "FUNCTION", "MESSAGE");
    for (size_t i = 0; i < g_point_count; i++) {
        const ytrace_point_t *p = &g_points[i];
        const char *bname = strrchr(p->file, '/');
        if (!bname) bname = strrchr(p->file, '\\');
        bname = bname ? bname + 1 : p->file;
        char loc_buf[32];
        snprintf(loc_buf, sizeof(loc_buf), "%s:%d", bname, p->line);
        char msg_buf[32];
        if (p->message && strlen(p->message) > 0) {
            snprintf(msg_buf, sizeof(msg_buf), "%.28s%s", p->message,
                     strlen(p->message) > 28 ? ".." : "");
        } else {
            msg_buf[0] = '\0';
        }
        fprintf(stderr, "%-4zu %-7s %-6s %-30s %-20s %s\n", i, *p->enabled ? "ON" : "off",
                p->level, loc_buf, p->function, msg_buf);
    }
    fprintf(stderr, "\n");
    YTRACE_UNLOCK();
}

void ytime_timer_observe(struct ytime_timer *t, const char *name, const char *file, int line,
                         const char *function, double elapsed_ms)
{
    YTRACE_LOCK();
    if (!t->registered) {
        t->name = name;
        t->file = file;
        t->line = line;
        t->function = function;
        t->count = 0;
        t->sum_ms = 0.0;
        t->last_ms = 0.0;
        t->min_ms = elapsed_ms;
        t->max_ms = elapsed_ms;
        t->avg_ms = 0.0;
        if (g_timer_count < YTIME_MAX_TIMERS) {
            g_timers[g_timer_count++] = t;
        } else {
            static bool warned = false;
            if (!warned) {
                fprintf(stderr, "[ytrace] WARNING: max timers (%d) exceeded\n", YTIME_MAX_TIMERS);
                warned = true;
            }
        }
        t->registered = true;
    }
    t->count++;
    t->sum_ms += elapsed_ms;
    t->last_ms = elapsed_ms;
    t->avg_ms = t->sum_ms / (double)t->count;
    if (elapsed_ms < t->min_ms) t->min_ms = elapsed_ms;
    if (elapsed_ms > t->max_ms) t->max_ms = elapsed_ms;
    YTRACE_UNLOCK();
}

size_t ytime_timer_get_count(void)
{
    YTRACE_LOCK();
    size_t count = g_timer_count;
    YTRACE_UNLOCK();
    return count;
}

const struct ytime_timer *const *ytime_timer_get_all(void)
{
    return (const struct ytime_timer *const *)g_timers;
}

void ytime_timer_list(void)
{
    YTRACE_LOCK();
    fprintf(stderr, "\n[ytrace] Registered timers: %zu\n", g_timer_count);
    fprintf(stderr, "%-4s %-20s %-30s %10s %10s %10s %10s %10s\n", "IDX", "NAME", "FILE:LINE",
            "N", "LAST(ms)", "AVG(ms)", "MIN(ms)", "MAX(ms)");
    for (size_t i = 0; i < g_timer_count; i++) {
        const struct ytime_timer *t = g_timers[i];
        const char *bname = strrchr(t->file, '/');
        if (!bname) bname = strrchr(t->file, '\\');
        bname = bname ? bname + 1 : t->file;
        char loc_buf[32];
        snprintf(loc_buf, sizeof(loc_buf), "%s:%d", bname, t->line);
        fprintf(stderr, "%-4zu %-20s %-30s %10llu %10.3f %10.3f %10.3f %10.3f\n", i,
                t->name ? t->name : "", loc_buf, (unsigned long long)t->count, t->last_ms,
                t->avg_ms, t->min_ms, t->max_ms);
    }
    fprintf(stderr, "\n");
    YTRACE_UNLOCK();
}

void ytime_timer_reset_all(void)
{
    YTRACE_LOCK();
    for (size_t i = 0; i < g_timer_count; i++) {
        struct ytime_timer *t = g_timers[i];
        t->count = 0;
        t->sum_ms = 0.0;
        t->last_ms = 0.0;
        t->min_ms = 0.0;
        t->max_ms = 0.0;
        t->avg_ms = 0.0;
    }
    YTRACE_UNLOCK();
}

#endif /* YTRACE_C_ENABLED */
