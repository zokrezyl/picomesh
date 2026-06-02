/* Private internal header for the standalone picoforge-webapp binary.
 * NOT installed; nothing outside src/picoforge/webapp/ includes this. */
#ifndef PICOFORGE_WEBAPP_INTERNAL_H
#define PICOFORGE_WEBAPP_INTERNAL_H

#include <picomesh/ycore/result.h>

struct yloop;

struct webapp_config {
    const char *gateway_url;    /* e.g. "http://127.0.0.1:8090" */
    const char *templates_dir;  /* may be NULL → no templates */
    const char *static_dir;     /* may be NULL → no static files */
};

/* Start the HTTP listener on host:port. Spawns serve coroutines via
 * yloop_listen_tcp. Does NOT block — returns after the listener is
 * bound; the caller runs yloop_run() to actually serve. */
struct picomesh_void_result webapp_start(struct yloop *loop,
                                         const char *host, int port,
                                         const struct webapp_config *cfg);

#endif
