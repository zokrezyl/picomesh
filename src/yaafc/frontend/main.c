/* yaafc-frontend — standalone HTML rendering process.
 *
 * Mirrors yaapp's scenarios/git-yaapp/frontend/frontend.py:
 *   - own HTTP listener for browsers
 *   - every backend call leaves this process as `POST /_rpc` against
 *     `--gateway-url` (yaafc gateway, yaapp gateway — same contract)
 *   - no yaafc plugins linked in, no mesh, no yclass dispatch — pure
 *     HTTP client + template renderer
 *
 * Auth: the `yaafc-sid` cookie value is forwarded to the gateway as a
 * `yaafc-sid` header on every outbound /_rpc call. Opaque token, no
 * JWT ever passes through.
 *
 * Usage:
 *   yaafc-frontend --gateway-url http://127.0.0.1:8080 \
 *                  --host 0.0.0.0 --port 8081 \
 *                  [--templates scenarios/git-yaafc/frontend/templates] \
 *                  [--static scenarios/git-yaafc/frontend/static]
 */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yargv/yargv.h>
#include <yaafc/yloop/yloop.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend.h"

static const struct yargv_option_def FE_OPTIONS[] = {
    {"--gateway-url", NULL, "gateway_url", "Gateway base URL (required)",
     YARGV_VALUE, 0},
    {"--host",        NULL, "host",         "Bind address (default 0.0.0.0)",
     YARGV_VALUE, 0},
    {"--port",        NULL, "port",         "Bind port (default 8081)",
     YARGV_VALUE, 0},
    {"--templates",   NULL, "templates",    "Template directory (default ./templates)",
     YARGV_VALUE, 0},
    {"--static",      NULL, "static_dir",   "Static directory (default ./static)",
     YARGV_VALUE, 0},
    {"--verbose",     "-v", "verbose",      "Enable debug logging",
     YARGV_BOOL,  0},
    {"--help",        "-h", "help",         "Show this message",
     YARGV_BOOL,  0},
};
#define FE_OPTION_COUNT (sizeof(FE_OPTIONS) / sizeof(FE_OPTIONS[0]))

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s --gateway-url <url> [--host <h>] [--port <p>]\n"
        "          [--templates <dir>] [--static <dir>] [-v]\n"
        "\n"
        "Standalone HTML frontend. Talks to the gateway via POST /_rpc.\n"
        "Works against either the yaafc C gateway or the yaapp Python gateway —\n"
        "the wire shape is identical.\n",
        prog);
}

static int die(const char *what, const char *why)
{
    fprintf(stderr, "yaafc-frontend: %s: %s\n", what, why);
    return 1;
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    struct yargv_chain_ptr_result pr =
        yargv_parse(FE_OPTIONS, FE_OPTION_COUNT, argc, argv);
    if (YAAFC_IS_ERR(pr)) {
        fprintf(stderr, "yaafc-frontend: argv parse: %s\n",
                pr.error.msg ? pr.error.msg : "?");
        yaafc_error_destroy(pr.error);
        return 2;
    }
    struct yargv_chain *cli = pr.value;

    if (yargv_get_bool(cli, "help", 0)) {
        usage(argv[0]);
        yargv_chain_destroy(cli);
        return 0;
    }
    if (yargv_get_bool(cli, "verbose", 0)) {
        ytrace_set_all_enabled(true);
    }

    const char *gateway_url = yargv_get_string(cli, "gateway_url", NULL);
    if (!gateway_url || !*gateway_url) {
        usage(argv[0]);
        yargv_chain_destroy(cli);
        return die("config", "--gateway-url is required");
    }
    const char *host = yargv_get_string(cli, "host", "0.0.0.0");
    int port = (int)yargv_get_int(cli, "port", 8081);
    const char *templates_dir = yargv_get_string(cli, "templates", "templates");
    const char *static_dir    = yargv_get_string(cli, "static_dir", "static");

    struct yloop_ptr_result lr = yloop_create();
    if (YAAFC_IS_ERR(lr)) {
        yargv_chain_destroy(cli);
        return die("yloop_create", lr.error.msg ? lr.error.msg : "?");
    }
    struct yloop *loop = lr.value;

    struct frontend_config fc = {
        .gateway_url = gateway_url,
        .templates_dir = templates_dir,
        .static_dir = static_dir,
    };
    struct yaafc_void_result sr = frontend_start(loop, host, port, &fc);
    if (YAAFC_IS_ERR(sr)) {
        fprintf(stderr, "yaafc-frontend: start: %s\n",
                sr.error.msg ? sr.error.msg : "?");
        yaafc_error_destroy(sr.error);
        yloop_destroy(loop);
        yargv_chain_destroy(cli);
        return 1;
    }

    yinfo("yaafc-frontend: listening on %s:%d (gateway=%s)",
          host, port, gateway_url);
    struct yaafc_void_result rr = yloop_run(loop);
    if (YAAFC_IS_ERR(rr)) yaafc_error_destroy(rr.error);

    yloop_destroy(loop);
    yargv_chain_destroy(cli);
    return 0;
}
