/* cli — one-shot local invoker.
 *
 * The CLI is *not* a server. It's a subcommand of the driver:
 *
 *     yaafc invoke <plugin>_<class>_<method> [arg1 arg2 ...]
 *
 * It creates a fresh local instance of the named class, parses
 * positional args as JSON scalars (numbers / true|false|null /
 * otherwise treated as strings), calls the local jinvoke, and prints
 * the JSON-encoded result on stdout.
 *
 * State is not persisted between invocations — analogous to yaapp's
 * `cli` runner which spins up a fresh proxy tree per call. For
 * persistent state, point a `yaafc invoke` at a running `yaafc yttp`
 * server through its JSON-RPC interface (separate feature). */

#ifndef YAAFC_FRONTENDS_CLI_CLI_H
#define YAAFC_FRONTENDS_CLI_CLI_H

struct yaafc_engine;

/* Dispatch the parsed CLI subcommand. Returns the process exit code:
 *   0  — success
 *   2  — usage error
 *   1  — invoke / lookup error
 *
 * Reads the subcommand + remaining argv from the engine's stored CLI
 * chain. */
int yaafc_cli_dispatch(struct yaafc_engine *e);

#endif /* YAAFC_FRONTENDS_CLI_CLI_H */
