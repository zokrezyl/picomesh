/* Shared raw-HTTP-header helpers for authenticator modules. Header-only
 * (static inline) so each authenticator translation unit gets its own copy
 * with no link coupling. Reads cookies / a named header / a bearer token out
 * of the raw header block the frontend hands the pipeline. */

#ifndef PICOMESH_AUTHENTICATORS_HTTP_UTIL_H
#define PICOMESH_AUTHENTICATORS_HTTP_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Copy the value of header `name` (case-insensitive) into out[cap]. 1 on hit. */
static inline int authn_header_value(const char *raw, size_t raw_len, const char *name,
                                     char *out, size_t cap)
{
    size_t name_len = strlen(name);
    const char *cursor = raw;
    const char *end = raw + raw_len;
    while (cursor < end) {
        const char *eol = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!eol) break;
        size_t line_len = (size_t)(eol - cursor);
        if (line_len && cursor[line_len - 1] == '\r') line_len--;
        if (line_len > name_len + 1 && cursor[name_len] == ':' &&
            strncasecmp(cursor, name, name_len) == 0) {
            const char *value = cursor + name_len + 1;
            while (value < cursor + line_len && (*value == ' ' || *value == '\t')) ++value;
            size_t value_len = (size_t)(cursor + line_len - value);
            if (value_len >= cap) value_len = cap - 1;
            memcpy(out, value, value_len);
            out[value_len] = 0;
            return 1;
        }
        cursor = eol + 1;
    }
    return 0;
}

/* Copy the value of cookie `name` into out[cap]. 1 on hit. */
static inline int authn_cookie_value(const char *raw, size_t raw_len, const char *name,
                                     char *out, size_t cap)
{
    out[0] = 0;
    size_t name_len = strlen(name);
    const char *cursor = raw;
    const char *end = raw + raw_len;
    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        if (!line_end) line_end = end;
        if (line_end - cursor > 7 && strncasecmp(cursor, "Cookie:", 7) == 0) {
            const char *scan = cursor + 7;
            while (scan < line_end && (*scan == ' ' || *scan == '\t')) ++scan;
            while (scan < line_end) {
                const char *eq = memchr(scan, '=', (size_t)(line_end - scan));
                if (!eq) break;
                if ((size_t)(eq - scan) == name_len && memcmp(scan, name, name_len) == 0) {
                    const char *value_start = eq + 1;
                    const char *value_end = value_start;
                    while (value_end < line_end && *value_end != ';' &&
                           *value_end != '\r' && *value_end != '\n')
                        ++value_end;
                    size_t value_len = (size_t)(value_end - value_start);
                    if (value_len >= cap) value_len = cap - 1;
                    memcpy(out, value_start, value_len);
                    out[value_len] = 0;
                    return 1;
                }
                const char *semi = memchr(scan, ';', (size_t)(line_end - scan));
                if (!semi) break;
                scan = semi + 1;
                while (scan < line_end && *scan == ' ') ++scan;
            }
        }
        if (line_end == end) break;
        cursor = line_end + 1;
    }
    return 0;
}

/* Copy the `Authorization: Bearer <token>` value into out[cap]. Returns 1 only
 * for a real `Bearer ` scheme — any other scheme (`Basic …`, etc.) is NOT a
 * bearer credential and yields no match, so it is left for whatever handles it
 * rather than forced into a 401. */
static inline int authn_bearer_token(const char *raw, size_t raw_len, char *out, size_t cap)
{
    char header[1100];
    if (!authn_header_value(raw, raw_len, "authorization", header, sizeof(header))) { out[0] = 0; return 0; }
    if (strncasecmp(header, "Bearer ", 7) != 0) { out[0] = 0; return 0; } /* not the Bearer scheme */
    const char *value = header + 7;
    while (*value == ' ') ++value;
    size_t len = strlen(value);
    if (!len || len >= cap) { out[0] = 0; return 0; }
    memcpy(out, value, len + 1);
    return 1;
}

/* Build a single-element JSON args array ["<value>"] with `value` JSON-escaped,
 * for invoking a configured lookup with a client-supplied token/sid. Returns 1
 * on success, 0 if it would overflow `out[cap]`. Shared by the authenticators
 * so neither hand-rolls escaping. */
static inline int authn_build_string_args(char *out, size_t cap, const char *value)
{
    size_t pos = 0;
    if (cap < 4) return 0;
    out[pos++] = '[';
    out[pos++] = '"';
    for (const char *p = value; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        const char *esc = NULL;
        char ubuf[8];
        if (c == '"') esc = "\\\"";
        else if (c == '\\') esc = "\\\\";
        else if (c < 0x20) { snprintf(ubuf, sizeof(ubuf), "\\u%04x", c); esc = ubuf; }
        if (esc) {
            size_t el = strlen(esc);
            if (pos + el + 2 >= cap) return 0;
            memcpy(out + pos, esc, el); pos += el;
        } else {
            if (pos + 3 >= cap) return 0;
            out[pos++] = (char)c;
        }
    }
    if (pos + 3 >= cap) return 0;
    out[pos++] = '"';
    out[pos++] = ']';
    out[pos] = 0;
    return 1;
}

#endif /* PICOMESH_AUTHENTICATORS_HTTP_UTIL_H */
