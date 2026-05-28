/* Private internal header for the standalone yaafc-frontend binary.
 * NOT installed; nothing outside src/yaafc/frontend/ includes this. */
#ifndef YAAFC_FRONTEND_INTERNAL_H
#define YAAFC_FRONTEND_INTERNAL_H

#include <yaafc/ycore/result.h>

struct yloop;

struct frontend_config {
    const char *gateway_url;    /* e.g. "http://127.0.0.1:8080" */
    const char *templates_dir;  /* may be NULL → no templates */
    const char *static_dir;     /* may be NULL → no static files */
};

/* Start the HTTP listener on host:port. Spawns serve coroutines via
 * yloop_listen_tcp. Does NOT block — returns after the listener is
 * bound; the caller runs yloop_run() to actually serve. */
struct yaafc_void_result frontend_start(struct yloop *loop,
                                        const char *host, int port,
                                        const struct frontend_config *cfg);

#endif
