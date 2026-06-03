/* `policy` authorizer — role-based access control from an endpoint policy
 * table. Default-deny, the GitLab role ladder, account resolution against the
 * call args, the `anonymous`/`authenticated` shortcuts, and the
 * `site:maintainer+` bypass. Port of the yaapp policy authorizer.
 *
 * Config:
 *   authorizer:
 *     type: policy
 *     policy:
 *       _describe: anonymous
 *       accounts.accounts.list: { required_role: owner, account_from: site }
 *       git_repo.git_repo.read_tree:
 *         { required_role: reporter, account_from: "{kwargs.name:account}" }
 *
 * A bare string is shorthand: `foo: owner` == `{required_role: owner,
 * account_from: site}`. Endpoints absent from the table are DENIED. */

#include <picomesh/authorizers/base.h>
#include <picomesh/yconfig/yconfig.h>
#include <picomesh/yjson/yjson.h>
#include <picomesh/yclass/jinvoke.h>
#include <picomesh/ysecurity/jwt.h>
#include <picomesh/ysecurity/jwt_verifier.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct policy_state {
    struct picomesh_engine *engine;
    const struct yconfig_node *policy; /* the `policy:` map (points into config) */
    struct picomesh_jwt_verifier *verifier;
};

static struct picomesh_void_ptr_result policy_create(struct picomesh_engine *engine,
                                                     const struct yconfig_node *config)
{
    struct policy_state *state = calloc(1, sizeof(*state));
    if (!state) return PICOMESH_ERR(picomesh_void_ptr, "policy: out of memory");
    state->engine = engine;
    state->policy = yconfig_node_get(config, "policy"); /* may be NULL → all default-deny */
    struct picomesh_void_ptr_result verifier = picomesh_jwt_verifier_create(engine);
    if (PICOMESH_IS_ERR(verifier)) { free(state); return PICOMESH_ERR(picomesh_void_ptr, "policy: verifier create failed", verifier); }
    state->verifier = verifier.value;
    return PICOMESH_OK(picomesh_void_ptr, state);
}

static struct picomesh_authz_decision decide(int allowed, int status, const char *reason)
{
    struct picomesh_authz_decision decision = {.allowed = allowed, .status = status};
    snprintf(decision.reason, sizeof(decision.reason), "%s", reason);
    return decision;
}

/* 1 if the claims `groups` array contains the exact membership string `group`
 * (e.g. "site:runner"). Used by `required_group` rules, which gate on an exact
 * group rather than the role ladder. */
static int groups_contains(const struct yjson_value *groups, const char *group)
{
    if (!groups || !yjson_is_array(groups) || !group) return 0;
    size_t n = yjson_array_size(groups);
    for (size_t i = 0; i < n; ++i) {
        const char *entry = yjson_as_string(yjson_array_at(groups, i), NULL);
        if (entry && strcmp(entry, group) == 0) return 1;
    }
    return 0;
}

/* Highest role rank held for `account` in the claims `groups` array, or -1. */
static int groups_best_rank(const struct yjson_value *groups, const char *account)
{
    if (!groups || !yjson_is_array(groups) || !account) return -1;
    size_t account_len = strlen(account);
    size_t n = yjson_array_size(groups);
    int best = -1;
    for (size_t i = 0; i < n; ++i) {
        const char *entry = yjson_as_string(yjson_array_at(groups, i), NULL);
        if (!entry) continue;
        const char *colon = strchr(entry, ':');
        if (!colon) continue;
        size_t slug_len = (size_t)(colon - entry);
        if (slug_len == account_len && memcmp(entry, account, account_len) == 0) {
            int rank = picomesh_role_rank(colon + 1);
            if (rank > best) best = rank;
        }
    }
    return best;
}

/* Resolve `account_from` against the positional `args`, using the method's
 * declared parameter names to honour yaapp's `{kwargs.name}` form. Supports:
 *   site                  -> "site"
 *   {kwargs.NAME}         -> the string value of param NAME
 *   {kwargs.NAME:account} -> that value's account part (before '/')
 *   arg:N                 -> the Nth positional arg (positional escape hatch)
 *   <literal>             -> taken as a literal account slug
 * Returns 1 on success. */
static int resolve_account(const char *account_from, const char *endpoint,
                           const struct yjson_value *args, char *out, size_t cap)
{
    if (!account_from || !*account_from) return 0;
    if (strcmp(account_from, "site") == 0) { snprintf(out, cap, "site"); return 1; }

    long index = -1;
    int account_part = 0;
    if (strncmp(account_from, "{kwargs.", 8) == 0) {
        const char *field = account_from + 8;
        const char *close = strchr(field, '}');
        if (!close) return 0;
        const char *colon = memchr(field, ':', (size_t)(close - field));
        size_t field_len = colon ? (size_t)(colon - field) : (size_t)(close - field);
        if (colon && strncmp(colon + 1, "account}", 8) == 0) account_part = 1;
        /* Map the param NAME to its positional index via the method's metadata. */
        char qname[192];
        snprintf(qname, sizeof(qname), "%s", endpoint);
        for (char *p = qname; *p; ++p) if (*p == '.') *p = '_';
        const struct jinvoke_params *params = jinvoke_params_for(qname);
        if (!params) return 0;
        for (size_t i = 0; i < params->count; ++i) {
            if (strlen(params->items[i].name) == field_len &&
                strncmp(params->items[i].name, field, field_len) == 0) { index = (long)i; break; }
        }
        if (index < 0) return 0;
    } else if (strncmp(account_from, "arg:", 4) == 0) {
        index = strtol(account_from + 4, NULL, 10);
    } else {
        snprintf(out, cap, "%s", account_from); /* literal slug */
        return 1;
    }

    if (index < 0 || !args || !yjson_is_array(args)) return 0;
    const char *value = yjson_as_string(yjson_array_at(args, (size_t)index), NULL);
    if (!value || !*value) return 0;
    if (account_part) {
        const char *slash = strchr(value, '/');
        size_t len = slash ? (size_t)(slash - value) : strlen(value);
        if (len >= cap) len = cap - 1;
        memcpy(out, value, len);
        out[len] = 0;
    } else {
        snprintf(out, cap, "%s", value);
    }
    return 1;
}

static struct picomesh_authz_decision policy_authorize(void *state_ptr, const char *endpoint,
                                                       const struct yjson_value *args, const char *jwt)
{
    struct policy_state *state = state_ptr;

    /* Schema reads collapse to a single policy entry regardless of depth. */
    const char *lookup_key = endpoint;
    size_t elen = strlen(endpoint);
    if (strcmp(endpoint, "_describe") == 0 ||
        (elen >= 10 && strcmp(endpoint + elen - 10, "._describe") == 0))
        lookup_key = "_describe";
    else if (strcmp(endpoint, "_describe_tree") == 0 ||
             (elen >= 15 && strcmp(endpoint + elen - 15, "._describe_tree") == 0))
        lookup_key = "_describe_tree";

    const struct yconfig_node *rule = state->policy ? yconfig_node_get(state->policy, lookup_key) : NULL;
    if (!rule)
        return decide(0, 403, "no policy entry for endpoint (default deny)");

    /* A rule is a bare role string (account_from defaults to "site"), or a map
     * carrying {required_role, account_from} and/or {required_group}.
     *   required_role  — checked against the role ladder for an account.
     *   required_group — an EXACT group membership (e.g. "site:runner"), off
     *                    the ladder, for non-hierarchical roles like runners. */
    const char *required_role = NULL;
    const char *account_from = "site";
    const char *required_group = NULL;
    if (yconfig_node_kind(rule) == YCONFIG_MAP) {
        required_role = yconfig_node_as_string(yconfig_node_get(rule, "required_role"), NULL);
        account_from = yconfig_node_as_string(yconfig_node_get(rule, "account_from"), "site");
        required_group = yconfig_node_as_string(yconfig_node_get(rule, "required_group"), NULL);
    } else {
        required_role = yconfig_node_as_string(rule, NULL);
    }
    int has_role = required_role && *required_role;
    int has_group = required_group && *required_group;
    if (!has_role && !has_group)
        return decide(0, 403, "policy entry has neither required_role nor required_group");

    if (has_role && strcmp(required_role, "anonymous") == 0)
        return decide(1, 0, "anonymous");

    if (!jwt || !*jwt)
        return decide(0, 401, "authentication required");

    struct picomesh_string_result claims_r = picomesh_jwt_verifier_verify(state->verifier, jwt);
    if (PICOMESH_IS_ERR(claims_r)) {
        picomesh_error_destroy(claims_r.error);
        return decide(0, 401, "invalid or expired token");
    }
    struct yjson_doc *claims_doc = yjson_parse(claims_r.value, strlen(claims_r.value));
    free(claims_r.value);
    if (!claims_doc) return decide(0, 401, "token claims not parseable");
    const struct yjson_value *groups = yjson_object_get(yjson_doc_root(claims_doc), "groups");

    struct picomesh_authz_decision result;

    /* required_group: exact membership. Must hold if specified. */
    if (has_group && !groups_contains(groups, required_group)) {
        result = decide(0, 403, "caller lacks the required group");
        goto done;
    }

    /* required_role: ladder check (skipped when only a group was required). */
    if (has_role) {
        if (strcmp(required_role, "authenticated") == 0) {
            result = decide(1, 0, "authenticated");
            goto done;
        }
        int need = picomesh_role_rank(required_role);
        if (need < 0) { result = decide(0, 403, "policy error: unknown required_role"); goto done; }

        /* Site-level bypass: maintainer+ on the synthetic `site` account. */
        if (groups_best_rank(groups, "site") >= picomesh_role_rank("maintainer")) {
            result = decide(1, 0, "site-level bypass");
            goto done;
        }

        char account[160];
        if (!resolve_account(account_from, endpoint, args, account, sizeof(account))) {
            result = decide(0, 403, "could not resolve account from call args");
            goto done;
        }
        result = groups_best_rank(groups, account) >= need
                     ? decide(1, 0, "role on account satisfies requirement")
                     : decide(0, 403, "insufficient role on account");
        goto done;
    }

    /* Only a required_group was specified, and it held. */
    result = decide(1, 0, "caller holds the required group");

done:
    yjson_doc_free(claims_doc);
    return result;
}

static void policy_destroy(void *state_ptr)
{
    struct policy_state *state = state_ptr;
    if (!state) return;
    picomesh_jwt_verifier_destroy(state->verifier);
    free(state);
}

const struct picomesh_authorizer_ops *picomesh_authorizer_policy_ops(void)
{
    static const struct picomesh_authorizer_ops ops = {
        .type_name = "policy",
        .create = policy_create,
        .authorize = policy_authorize,
        .destroy = policy_destroy,
    };
    return &ops;
}
