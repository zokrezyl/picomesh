/* yhttp — HTTP/1.1 + JSON frontend (yaapp.fastapi equivalent).
 *
 * Routes:
 *
 *   POST /create
 *     body  {"class": "<plugin>_<class>"}
 *     reply {"handle": <u64>}
 *
 *   POST /invoke
 *     body  {"method": "<qname>", "handle": <u64>, "args": [...]}
 *     reply {"result": <value>}
 *
 *   GET /describe?class=NAME
 *     reply {"class": "...", "methods": [...]}
 *
 *   GET /
 *     reply text/plain root page listing routes.
 *
 * Errors come back as
 *   {"error": {"code": <int>, "message": "<str>"}}
 * with appropriate HTTP status (400 / 404 / 500).
 *
 * One coroutine per accepted peer. HTTP/1.1 keep-alive supported on
 * connections that send `Connection: keep-alive`; the default is close
 * after one request, matching what curl's default does. */

#ifndef YAAFC_FRONTENDS_YHTTP_YHTTP_H
#define YAAFC_FRONTENDS_YHTTP_YHTTP_H

#include <yaafc/ycore/result.h>

struct yaafc_engine;
struct yhttp_frontend;

YAAFC_RESULT_DECLARE(yhttp_frontend_ptr, struct yhttp_frontend *);

struct yhttp_config {
    const char *host; /* default 127.0.0.1 */
    int port;         /* default 8080 */
};

struct yhttp_frontend_ptr_result yhttp_start(struct yaafc_engine *e,
                                             const struct yhttp_config *cfg);
void yhttp_stop(struct yhttp_frontend *f);

#endif /* YAAFC_FRONTENDS_YHTTP_YHTTP_H */
