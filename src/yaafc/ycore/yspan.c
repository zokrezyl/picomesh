/* yspan — in-memory trace-span collector (see yspan.h). */

#include <yaafc/ycore/yspan.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define YSPAN_MAX 65536
#define YSPAN_OP_MAX 48

struct yspan_entry {
    char op[YSPAN_OP_MAX];
    double dur_us;
};

struct yspan_state {
    struct yspan_entry e[YSPAN_MAX];
    size_t n;       /* total recorded (capped at YSPAN_MAX) */
    int overflow;   /* set once we stop appending */
};

/* Process-global singleton. Spans are now recorded from the loop thread
 * AND from worker-pool threads (the gateway offloads forwards there), so
 * a small lock guards the ring. */
static struct yspan_state *yspan_state(void)
{
    static struct yspan_state s = {0};
    return &s;
}

static pthread_mutex_t *yspan_lock(void)
{
    static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    return &mu;
}

void yspan_record(const char *op, double dur_us)
{
    struct yspan_state *s = yspan_state();
    pthread_mutex_lock(yspan_lock());
    if (s->n >= YSPAN_MAX) { s->overflow = 1; pthread_mutex_unlock(yspan_lock()); return; }
    struct yspan_entry *e = &s->e[s->n++];
    size_t i = 0;
    if (op) for (; op[i] && i < YSPAN_OP_MAX - 1; ++i) e->op[i] = op[i];
    e->op[i] = 0;
    e->dur_us = dur_us;
    pthread_mutex_unlock(yspan_lock());
}

void yspan_reset(void)
{
    struct yspan_state *s = yspan_state();
    pthread_mutex_lock(yspan_lock());
    s->n = 0;
    s->overflow = 0;
    pthread_mutex_unlock(yspan_lock());
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double pctl(const double *sorted, size_t n, double p)
{
    if (!n) return 0.0;
    size_t idx = (size_t)(p / 100.0 * (double)(n - 1) + 0.5);
    if (idx >= n) idx = n - 1;
    return sorted[idx];
}

size_t yspan_dump(char *buf, size_t cap)
{
    struct yspan_state *s = yspan_state();
    pthread_mutex_lock(yspan_lock());
    size_t off = 0;
    int w = snprintf(buf + off, cap - off,
        "%-34s %8s %9s %9s %9s %9s\n",
        "op", "count", "p50_us", "p90_us", "p99_us", "max_us");
    if (w > 0) off += (size_t)w;

    /* Distinct ops, in first-seen order. */
    char ops[256][YSPAN_OP_MAX];
    size_t op_count = 0;
    for (size_t i = 0; i < s->n; ++i) {
        int found = 0;
        for (size_t j = 0; j < op_count; ++j)
            if (strcmp(ops[j], s->e[i].op) == 0) { found = 1; break; }
        if (!found && op_count < 256) {
            memcpy(ops[op_count], s->e[i].op, YSPAN_OP_MAX);
            op_count++;
        }
    }

    double *durs = malloc(s->n ? s->n * sizeof(double) : 1);
    if (!durs) { if (off < cap) buf[off] = 0; pthread_mutex_unlock(yspan_lock()); return off; }

    for (size_t j = 0; j < op_count; ++j) {
        size_t k = 0;
        for (size_t i = 0; i < s->n; ++i)
            if (strcmp(s->e[i].op, ops[j]) == 0) durs[k++] = s->e[i].dur_us;
        qsort(durs, k, sizeof(double), cmp_double);
        if (off < cap) {
            w = snprintf(buf + off, cap - off,
                "%-34s %8zu %9.0f %9.0f %9.0f %9.0f\n",
                ops[j], k, pctl(durs, k, 50), pctl(durs, k, 90),
                pctl(durs, k, 99), durs[k ? k - 1 : 0]);
            if (w > 0) off += (size_t)w;
        }
    }
    free(durs);

    if (s->overflow && off < cap)
        off += (size_t)snprintf(buf + off, cap - off,
                                "(ring full at %d spans — older not shown)\n", YSPAN_MAX);
    if (off < cap) buf[off] = 0;
    else if (cap) buf[cap - 1] = 0;
    pthread_mutex_unlock(yspan_lock());
    return off;
}
