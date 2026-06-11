/* picoforge runner agent — standalone CI runner (docs/runner-agent.md).
 *
 * An INDEPENDENT executable, not a mesh plugin: it authenticates to the
 * gateway with an opaque `rnr_<secret>` bearer token, registers itself, then
 * poll-leases pipeline jobs, executes them locally, streams logs back, and
 * reports completion. The gateway never connects to the agent — the agent is
 * always the client, so this works through NAT/firewalls with no inbound
 * listener, and it builds anywhere a C compiler + `curl` exist (Linux, macOS,
 * Windows) with no dependency on the mesh, libuv, or raw sockets.
 *
 * Transport is the `curl` binary (spawned per request), deliberately isolated
 * from the RPC envelope and job execution — pointing the agent at an https://
 * gateway, or adding a proxy / custom CA, is a matter of curl flags. JSON is
 * the only linked dependency (vendored yyjson).
 *
 * This is the C port of tools/picoforge/runner-agent/runner-agent.py. The
 * GitHub Actions workflow engine is not bundled; workflow jobs degrade
 * gracefully (logged + reported failed), the same as the Python runner when
 * its pyyaml engine is absent. */

/* popen/pclose and nanosleep are POSIX, not ISO C — request them before any
 * libc header is pulled in. (No effect on the _WIN32 path, which uses _popen
 * and Sleep.) */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <yyjson.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <process.h>
#define popen _popen
#define pclose _pclose
#define getpid _getpid
#define DEFAULT_SHELL_PREFIX "cmd /c "
#else
#include <unistd.h>
#define DEFAULT_SHELL_PREFIX "/bin/sh -c "
#endif

/* ---- options ---------------------------------------------------------- */

struct options {
  const char *gateway;     /* base URL, e.g. http://127.0.0.1:8090 */
  const char *token;       /* opaque rnr_ bearer token */
  long runner_id;          /* runner id from create_token */
  const char *name;        /* display name */
  const char *labels;      /* comma-separated capabilities */
  const char *version;     /* runner agent version */
  const char *host;        /* host info */
  const char *curl;        /* curl binary path */
  double lease_wait;       /* long-poll hold seconds (0 = short poll) */
  double http_timeout;     /* per-request curl budget for non-lease calls */
  double heartbeat;        /* heartbeat interval seconds */
  bool once;               /* lease/run one job then exit */
};

/* ---- small utilities -------------------------------------------------- */

static double monotonic_sec(void) {
  /* Wall-clock seconds. 1 s resolution is ample for the coarse cadences here
   * (heartbeat / backoff) and time() is ISO C — portable everywhere. (clock()
   * would be wrong: it measures CPU time, near-zero on an idle runner.) */
  return (double)time(NULL);
}

static void sleep_seconds(double seconds) {
  if (seconds <= 0)
    return;
#if defined(_WIN32)
  Sleep((unsigned long)(seconds * 1000.0));
#else
  struct timespec req;
  req.tv_sec = (time_t)seconds;
  req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1e9);
  nanosleep(&req, NULL);
#endif
}

/* A unique-enough temp file path for one curl exchange. */
static void temp_path(char *buf, size_t cap, const char *tag) {
  const char *dir = getenv("TMPDIR");
  if (!dir || !*dir)
    dir = getenv("TEMP");
  if (!dir || !*dir)
    dir = getenv("TMP");
  if (!dir || !*dir)
    dir = ".";
  static unsigned counter = 0;
  snprintf(buf, cap, "%s/picorunner-%ld-%u-%s", dir, (long)getpid(), counter++,
           tag);
}

static char *read_file(const char *path, size_t *out_len) {
  FILE *handle = fopen(path, "rb");
  if (!handle)
    return NULL;
  if (fseek(handle, 0, SEEK_END) != 0) {
    fclose(handle);
    return NULL;
  }
  long size = ftell(handle);
  if (size < 0) {
    fclose(handle);
    return NULL;
  }
  rewind(handle);
  char *data = (char *)malloc((size_t)size + 1);
  if (!data) {
    fclose(handle);
    return NULL;
  }
  size_t got = fread(data, 1, (size_t)size, handle);
  fclose(handle);
  data[got] = '\0';
  if (out_len)
    *out_len = got;
  return data;
}

/* ---- HTTP transport (curl subprocess) --------------------------------- */

/* POST `body` to <gateway>/_rpc with the bearer token. On success returns the
 * response body (caller frees) and sets *status; on transport failure returns
 * NULL. The request body goes to a temp file (curl --data-binary @file), the
 * response to another temp file (curl -o), and the HTTP status comes back on
 * curl's stdout (curl -w %{http_code}) — so no bidirectional pipe is needed
 * and the same code path works under POSIX popen and Windows _popen. */
static char *http_post_rpc(const struct options *opts, const char *body,
                           double timeout, long *status) {
  char request_path[1024];
  char response_path[1024];
  temp_path(request_path, sizeof(request_path), "req");
  temp_path(response_path, sizeof(response_path), "resp");

  FILE *request_file = fopen(request_path, "wb");
  if (!request_file)
    return NULL;
  fwrite(body, 1, strlen(body), request_file);
  fclose(request_file);

  /* curl writes the body to response_path and prints only the status code. */
  char command[4096];
  snprintf(command, sizeof(command),
           "%s --silent --show-error --max-time %.1f "
           "--output \"%s\" --write-out \"%%{http_code}\" "
           "--request POST --data-binary \"@%s\" "
           "--header \"Content-Type: application/json\" "
           "--header \"Authorization: Bearer %s\" \"%s/_rpc\"",
           opts->curl, timeout, response_path, request_path, opts->token,
           opts->gateway);

  FILE *pipe = popen(command, "r");
  if (!pipe) {
    remove(request_path);
    return NULL;
  }
  char code_text[32] = {0};
  if (!fgets(code_text, sizeof(code_text), pipe))
    code_text[0] = '\0';
  int curl_rc = pclose(pipe);
  remove(request_path);

  if (curl_rc != 0) {
    remove(response_path);
    fprintf(stderr, "[runner] curl failed (exit %d)\n", curl_rc);
    return NULL;
  }
  *status = strtol(code_text, NULL, 10);

  size_t response_len = 0;
  char *response = read_file(response_path, &response_len);
  remove(response_path);
  return response;
}

/* ---- RPC envelope ----------------------------------------------------- */

/* Build {"path": path, "args": <args>} and POST it. `args` is a mutable array
 * value belonging to `doc`. On success returns the parsed response document
 * (caller frees) with the "result" value reachable via yyjson_obj_get; on
 * failure returns NULL. */
static yyjson_doc *rpc_call(const struct options *opts, const char *path,
                            yyjson_mut_doc *doc, yyjson_mut_val *args,
                            double timeout) {
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);
  yyjson_mut_obj_add_str(doc, root, "path", path);
  yyjson_mut_obj_add_val(doc, root, "args", args);

  char *body = yyjson_mut_write(doc, 0, NULL);
  if (!body)
    return NULL;

  long status = 0;
  char *response = http_post_rpc(opts, body, timeout, &status);
  free(body);
  if (!response) {
    fprintf(stderr, "[runner] %s: transport error\n", path);
    return NULL;
  }
  if (status != 200) {
    fprintf(stderr, "[runner] %s: HTTP %ld: %.200s\n", path, status, response);
    free(response);
    return NULL;
  }
  yyjson_doc *parsed = yyjson_read(response, strlen(response), 0);
  free(response);
  if (!parsed) {
    fprintf(stderr, "[runner] %s: malformed response\n", path);
    return NULL;
  }
  return parsed;
}

/* append_log(job_id, offset, line) — fire-and-forget log streaming. */
static void rpc_append_log(const struct options *opts, long job_id,
                           long offset, const char *line) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *args = yyjson_mut_arr(doc);
  yyjson_mut_arr_add_int(doc, args, job_id);
  yyjson_mut_arr_add_int(doc, args, offset);
  yyjson_mut_arr_add_str(doc, args, line);
  yyjson_doc *reply =
      rpc_call(opts, "git_pipeline.git_pipeline.append_log", doc, args,
               opts->http_timeout);
  yyjson_mut_doc_free(doc);
  if (reply)
    yyjson_doc_free(reply);
}

/* ---- job execution ---------------------------------------------------- */

static const char *descriptor_str(yyjson_val *descriptor, const char *key,
                                  const char *fallback) {
  yyjson_val *value = yyjson_obj_get(descriptor, key);
  const char *text = value ? yyjson_get_str(value) : NULL;
  return (text && *text) ? text : fallback;
}

static bool descriptor_is_workflow(yyjson_val *descriptor) {
  if (descriptor_str(descriptor, "workflow_content", NULL))
    return true;
  const char *path = descriptor_str(descriptor, "pipeline_path", "");
  size_t len = strlen(path);
  bool yaml = (len >= 4 && strcmp(path + len - 4, ".yml") == 0) ||
              (len >= 5 && strcmp(path + len - 5, ".yaml") == 0);
  return yaml && strstr(path, "workflows") != NULL;
}

/* Run the leased job, streaming combined stdout/stderr to append_log line by
 * line. Returns 0 on success, 1 on failure. */
static int execute_job(const struct options *opts, yyjson_val *descriptor) {
  yyjson_val *job_id_val = yyjson_obj_get(descriptor, "job_id");
  long job_id = job_id_val ? (long)yyjson_get_int(job_id_val) : 0;
  long offset = 0;

  if (descriptor_is_workflow(descriptor)) {
    /* The GitHub Actions engine isn't bundled in the native runner yet (the
     * Python runner uses gha.py). Degrade gracefully, matching that path. */
    rpc_append_log(opts, job_id, offset,
                   "[runner] workflow engine not available in the native "
                   "runner\n");
    return 1;
  }

  const char *pipeline_path = descriptor_str(descriptor, "pipeline_path", "");
  const char *ref = descriptor_str(descriptor, "ref", "(none)");

  char command[4096];
  FILE *probe = pipeline_path[0] ? fopen(pipeline_path, "rb") : NULL;
  if (probe) {
    fclose(probe);
    /* Combined output via 2>&1; popen runs it through the platform shell. */
    snprintf(command, sizeof(command), "\"%s\" 2>&1", pipeline_path);
  } else {
    /* Built-in stub so lease -> execute -> log -> complete works end to end
     * even with no pipeline file present. `&&` sequences in both /bin/sh and
     * cmd.exe (a bare `&` would background under sh). */
    snprintf(command, sizeof(command),
             "echo [runner] job %ld ref %s && "
             "echo [runner] no pipeline file - running stub build && "
             "echo [runner] step 1/2 prepare && "
             "echo [runner] step 2/2 build OK",
             job_id, ref);
  }

  FILE *pipe = popen(command, "r");
  if (!pipe) {
    rpc_append_log(opts, job_id, offset, "[runner] failed to start job\n");
    return 1;
  }
  char line[8192];
  while (fgets(line, sizeof(line), pipe)) {
    rpc_append_log(opts, job_id, offset, line);
    offset += (long)strlen(line);
  }
  int exit_code = pclose(pipe);
  return exit_code == 0 ? 0 : 1;
}

/* ---- main loop -------------------------------------------------------- */

static int run(const struct options *opts) {
  /* register(runner_id, name, labels, version, host) */
  {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *args = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, args, opts->runner_id);
    yyjson_mut_arr_add_str(doc, args, opts->name);
    yyjson_mut_arr_add_str(doc, args, opts->labels);
    yyjson_mut_arr_add_str(doc, args, opts->version);
    yyjson_mut_arr_add_str(doc, args, opts->host);
    yyjson_doc *reply = rpc_call(opts, "runner_agent.runner_agent.register",
                                 doc, args, opts->http_timeout);
    yyjson_mut_doc_free(doc);
    if (!reply) {
      fprintf(stderr, "[runner] registration failed\n");
      return 1;
    }
    yyjson_doc_free(reply);
  }

  long long long_poll_ms = (long long)(opts->lease_wait > 0 ? opts->lease_wait * 1000.0 : 0);
  double lease_timeout =
      long_poll_ms > 0 ? opts->lease_wait + 15.0 : opts->http_timeout;
  printf("[runner] registered runner_id=%ld labels=%s lease=%s\n",
         opts->runner_id, opts->labels,
         long_poll_ms > 0 ? "long-poll" : "short-poll");
  fflush(stdout);

  double last_heartbeat = 0.0;
  int empty_polls = 0;
  const double backoff[] = {1.0, 2.0, 5.0, 15.0};

  for (;;) {
    double now = monotonic_sec();
    if (now - last_heartbeat >= opts->heartbeat) {
      yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
      yyjson_mut_val *args = yyjson_mut_arr(doc);
      yyjson_mut_arr_add_int(doc, args, opts->runner_id);
      yyjson_mut_arr_add_str(doc, args, "online");
      yyjson_doc *reply = rpc_call(opts, "runner_agent.runner_agent.heartbeat",
                                   doc, args, opts->http_timeout);
      yyjson_mut_doc_free(doc);
      if (reply)
        yyjson_doc_free(reply);
      last_heartbeat = now;
    }

    /* lease_job(runner_id, labels, wait_ms) */
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *args = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_int(doc, args, opts->runner_id);
    yyjson_mut_arr_add_str(doc, args, opts->labels);
    yyjson_mut_arr_add_int(doc, args, long_poll_ms);
    yyjson_doc *reply = rpc_call(opts, "git_pipeline.git_pipeline.lease_job",
                                 doc, args, lease_timeout);
    yyjson_mut_doc_free(doc);

    if (!reply) {
      sleep_seconds(opts->lease_wait > 0 ? 1.0 : 1.0);
      continue;
    }

    yyjson_val *descriptor = yyjson_obj_get(yyjson_doc_get_root(reply), "result");
    bool have_job = descriptor && yyjson_is_obj(descriptor) &&
                    yyjson_obj_get(descriptor, "job_id");

    if (!have_job) {
      yyjson_doc_free(reply);
      if (opts->once) {
        printf("[runner] --once: no job available, exiting\n");
        return 0;
      }
      if (long_poll_ms > 0) {
        /* Gateway already held the connection; re-issue immediately. */
        continue;
      }
      empty_polls++;
      int idx = empty_polls < 4 ? empty_polls : 3;
      sleep_seconds(backoff[idx]);
      continue;
    }

    empty_polls = 0;
    long job_id = (long)yyjson_get_int(yyjson_obj_get(descriptor, "job_id"));
    printf("[runner] leased job %ld\n", job_id);
    fflush(stdout);

    int status = execute_job(opts, descriptor);
    yyjson_doc_free(reply);

    /* complete_job(job_id, status, summary) */
    yyjson_mut_doc *cdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *cargs = yyjson_mut_arr(cdoc);
    yyjson_mut_arr_add_int(cdoc, cargs, job_id);
    yyjson_mut_arr_add_int(cdoc, cargs, status);
    yyjson_mut_arr_add_str(cdoc, cargs, status == 0 ? "succeeded" : "failed");
    yyjson_doc *creply = rpc_call(opts, "git_pipeline.git_pipeline.complete_job",
                                  cdoc, cargs, opts->http_timeout);
    yyjson_mut_doc_free(cdoc);
    if (creply)
      yyjson_doc_free(creply);
    printf("[runner] completed job %ld: %s\n", job_id,
           status == 0 ? "succeeded" : "failed");
    fflush(stdout);

    if (opts->once)
      return 0;
  }
}

/* ---- argument parsing ------------------------------------------------- */

static const char *env_or(const char *name, const char *fallback) {
  const char *value = getenv(name);
  return (value && *value) ? value : fallback;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s --token rnr_... --runner-id N [options]\n"
          "  --gateway URL       gateway base URL (env PICOFORGE_GATEWAY)\n"
          "  --token TOKEN        opaque rnr_ token (env PICOFORGE_RUNNER_TOKEN)\n"
          "  --runner-id N        runner id (env PICOFORGE_RUNNER_ID)\n"
          "  --name NAME          display name\n"
          "  --labels a,b         comma-separated capabilities\n"
          "  --version V          runner version\n"
          "  --host H             host info\n"
          "  --lease-wait S       long-poll hold seconds, 0 = short poll\n"
          "  --http-timeout S     per-request curl budget\n"
          "  --heartbeat S        heartbeat interval seconds\n"
          "  --curl PATH          curl binary (env PICOFORGE_CURL)\n"
          "  --once               lease/run one job then exit\n",
          argv0);
}

int main(int argc, char **argv) {
  struct options opts;
  opts.gateway = env_or("PICOFORGE_GATEWAY", "http://127.0.0.1:8090");
  opts.token = env_or("PICOFORGE_RUNNER_TOKEN", NULL);
  const char *runner_id_text = env_or("PICOFORGE_RUNNER_ID", NULL);
  opts.name = "picoforge-runner";
  opts.labels = "linux,x86_64";
  opts.version = "0.1.0";
  opts.host = NULL;
  opts.curl = env_or("PICOFORGE_CURL", "curl");
  opts.lease_wait = strtod(env_or("PICOFORGE_LEASE_WAIT", "25"), NULL);
  opts.http_timeout = 15.0;
  opts.heartbeat = 15.0;
  opts.once = false;

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;
#define NEED_VALUE()                                                            \
  do {                                                                          \
    if (!next) {                                                                \
      fprintf(stderr, "%s: missing value\n", arg);                             \
      return 2;                                                                 \
    }                                                                           \
    i++;                                                                        \
  } while (0)
    if (strcmp(arg, "--gateway") == 0) { NEED_VALUE(); opts.gateway = next; }
    else if (strcmp(arg, "--token") == 0) { NEED_VALUE(); opts.token = next; }
    else if (strcmp(arg, "--runner-id") == 0) { NEED_VALUE(); runner_id_text = next; }
    else if (strcmp(arg, "--name") == 0) { NEED_VALUE(); opts.name = next; }
    else if (strcmp(arg, "--labels") == 0) { NEED_VALUE(); opts.labels = next; }
    else if (strcmp(arg, "--version") == 0) { NEED_VALUE(); opts.version = next; }
    else if (strcmp(arg, "--host") == 0) { NEED_VALUE(); opts.host = next; }
    else if (strcmp(arg, "--curl") == 0) { NEED_VALUE(); opts.curl = next; }
    else if (strcmp(arg, "--lease-wait") == 0) { NEED_VALUE(); opts.lease_wait = strtod(next, NULL); }
    else if (strcmp(arg, "--http-timeout") == 0) { NEED_VALUE(); opts.http_timeout = strtod(next, NULL); }
    else if (strcmp(arg, "--heartbeat") == 0) { NEED_VALUE(); opts.heartbeat = strtod(next, NULL); }
    else if (strcmp(arg, "--once") == 0) { opts.once = true; }
    else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) { usage(argv[0]); return 0; }
    else { fprintf(stderr, "%s: unknown argument\n", arg); usage(argv[0]); return 2; }
#undef NEED_VALUE
  }

  if (!opts.token) {
    fprintf(stderr, "error: a runner token is required (--token / PICOFORGE_RUNNER_TOKEN)\n");
    return 2;
  }
  if (!runner_id_text) {
    fprintf(stderr, "error: a runner id is required (--runner-id / PICOFORGE_RUNNER_ID)\n");
    return 2;
  }
  opts.runner_id = strtol(runner_id_text, NULL, 10);
  if (!opts.host)
    opts.host = opts.name;

  return run(&opts);
}
