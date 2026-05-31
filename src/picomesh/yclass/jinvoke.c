/* jinvoke — JSON-aware method invocation registry.
 *
 * Linked list of per-module lookup hooks; the first non-NULL hit
 * wins. Each codegen-emitted module registers its own hook via
 * `jinvoke_add_lookup` from a __attribute__((constructor)). */

#include <picomesh/yclass/jinvoke.h>

#include <stdlib.h>

struct lookup_node {
    jinvoke_lookup_fn fn;
    struct lookup_node *next;
};

static struct lookup_node **chain_head(void)
{
    static struct lookup_node *head = NULL;
    return &head;
}

void jinvoke_add_lookup(jinvoke_lookup_fn fn)
{
    if (!fn) return;
    struct lookup_node *node = calloc(1, sizeof(*node));
    if (!node) return;
    struct lookup_node **head = chain_head();
    node->fn = fn;
    node->next = *head;
    *head = node;
}

jinvoke_fn jinvoke_for(const char *qname)
{
    if (!qname) return NULL;
    for (struct lookup_node *n = *chain_head(); n; n = n->next) {
        jinvoke_fn f = n->fn(qname);
        if (f) return f;
    }
    return NULL;
}
