/* cli — one-shot local invoker.
 *
 * Drives one method call against a freshly-allocated local instance.
 * Argv layout (after parsing by yargv):
 *
 *     yaafc invoke <plugin>_<class>_<method> [arg1 arg2 ...]
 *
 * Argument parsing for positionals is intentionally tiny:
 *   - "true" / "false" / "null"   → JSON literal
 *   - leading digit / sign + digits → JSON number (int or float)
 *   - anything else                  → JSON string
 *
 * That keeps shell quoting straightforward — `yaafc invoke
 * storage_kv_set 1 42` sends `[1, 42]`. */

#include <yaafc/frontends/cli/cli.h>
#include <yaafc/yengine/engine.h>
#include <yaafc/yargv/yargv.h>
#include <yaafc/yclass/class.h>
#include <yaafc/yclass/jinvoke.h>
#include <yaafc/yjson/yjson.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a JSON args array from positional argv. Returns a freshly
 * allocated buffer (caller frees) containing JSON like `[1, "foo"]`. */
static char *build_args_json(int argc, char *const *argv, size_t *out_len)
{
    struct yjson_writer *w = yjson_writer_new();
    yjson_w_begin_array(w);
    for (int i = 0; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "true") == 0) {
            yjson_w_bool(w, 1);
        } else if (strcmp(a, "false") == 0) {
            yjson_w_bool(w, 0);
        } else if (strcmp(a, "null") == 0) {
            yjson_w_null(w);
        } else {
            /* Numeric literal? Walk: optional sign, then digits, with
             * optional one '.' for float. */
            const char *p = a;
            int has_digit = 0, has_dot = 0, ok = 1;
            if (*p == '+' || *p == '-') ++p;
            for (; *p; ++p) {
                if (isdigit((unsigned char)*p)) { has_digit = 1; }
                else if (*p == '.' && !has_dot) { has_dot = 1; }
                else { ok = 0; break; }
            }
            if (ok && has_digit) {
                if (has_dot) yjson_w_float(w, strtod(a, NULL));
                else         yjson_w_int(w, (int64_t)strtoll(a, NULL, 10));
            } else {
                yjson_w_string(w, a);
            }
        }
    }
    yjson_w_end_array(w);
    size_t len;
    const char *data = yjson_w_data(w, &len);
    char *copy = malloc(len + 1);
    if (!copy) { yjson_writer_free(w); return NULL; }
    memcpy(copy, data, len + 1);
    if (out_len) *out_len = len;
    yjson_writer_free(w);
    return copy;
}

/* Derive a class qname from a method qname by walking back from the
 * end to the second-to-last underscore split. Convention is
 * `<plugin>_<class>_<verb...>` where `<plugin>_<class>` is the class
 * qname. We don't know where the verb starts without metadata, so we
 * try progressively shorter suffixes until class_by_name finds one. */
static char *guess_class_qname(const char *method_qname)
{
    char buf[256];
    size_t l = strlen(method_qname);
    if (l >= sizeof(buf)) return NULL;
    memcpy(buf, method_qname, l + 1);
    /* Walk from right to left, replacing each underscore with '\0'
     * and asking class_by_name if that prefix is a known class. */
    for (size_t i = l; i > 0; --i) {
        if (buf[i - 1] != '_') continue;
        buf[i - 1] = '\0';
        struct class_ptr_result r = class_by_name(buf);
        if (YAAFC_IS_OK(r) && r.value) {
            return strdup(buf);
        }
        if (YAAFC_IS_ERR(r)) yaafc_error_destroy(r.error);
        buf[i - 1] = '_';
    }
    return NULL;
}

int yaafc_cli_dispatch(struct yaafc_engine *e)
{
    struct yargv_chain *cli = yaafc_engine_cli(e);
    const char *sub = yargv_subcommand(cli);
    if (!sub || strcmp(sub, "invoke") != 0) {
        fprintf(stderr, "yaafc cli: expected 'invoke' subcommand\n");
        return 2;
    }
    int sargc = yargv_sub_argc(cli);
    char *const *sargv = yargv_sub_argv(cli);
    if (sargc < 1) {
        fprintf(stderr,
                "usage: yaafc invoke <plugin>_<class>_<method> [args...]\n");
        return 2;
    }
    const char *method = sargv[0];

    char *class_qname = guess_class_qname(method);
    if (!class_qname) {
        fprintf(stderr,
                "cli: cannot find a class matching method '%s'.\n"
                "     Try `yaafc <method-qname> with the class linked in.\n",
                method);
        return 1;
    }

    struct class_ptr_result cr = class_by_name(class_qname);
    if (YAAFC_IS_ERR(cr)) {
        fprintf(stderr, "cli: class_by_name(%s) failed\n", class_qname);
        yaafc_error_destroy(cr.error);
        free(class_qname);
        return 1;
    }
    struct object_ptr_result orr = object_alloc(cr.value);
    if (YAAFC_IS_ERR(orr)) {
        fprintf(stderr, "cli: object_alloc failed\n");
        yaafc_error_destroy(orr.error);
        free(class_qname);
        return 1;
    }
    struct object *obj = orr.value;

    /* Build args + parse them back via yjson so jinvoke sees a real
     * yjson_value*. The intermediate JSON text round-trip is wasteful
     * but keeps the jinvoke contract uniform with yttp's. */
    size_t alen;
    char *args_json = build_args_json(sargc - 1, sargv + 1, &alen);
    if (!args_json) {
        free(class_qname);
        object_free(obj);
        return 1;
    }
    struct yjson_doc *adoc = yjson_parse(args_json, alen);
    if (!adoc) {
        fprintf(stderr, "cli: failed to encode args: %s\n", yjson_last_error());
        free(args_json);
        free(class_qname);
        object_free(obj);
        return 1;
    }

    jinvoke_fn fn = jinvoke_for(method);
    if (!fn) {
        fprintf(stderr, "cli: no jinvoke registered for '%s'\n", method);
        yjson_doc_free(adoc);
        free(args_json);
        free(class_qname);
        object_free(obj);
        return 1;
    }

    struct yjson_writer *w = yjson_writer_new();
    char err[256] = {0};
    int rc = fn(obj, yjson_doc_root(adoc), w, err, sizeof(err));
    if (rc != 0) {
        fprintf(stderr, "cli: %s\n", err[0] ? err : "invoke failed");
    } else {
        size_t out_len;
        const char *out = yjson_w_data(w, &out_len);
        printf("%.*s\n", (int)out_len, out);
    }
    yjson_writer_free(w);
    yjson_doc_free(adoc);
    free(args_json);
    free(class_qname);
    object_free(obj);
    return rc == 0 ? 0 : 1;
}
