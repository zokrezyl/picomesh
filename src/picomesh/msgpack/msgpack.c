#include <picomesh/msgpack/msgpack.h>

#include <string.h>

/* cmp stores the user cursor in ctx->buf and drives all IO through these
 * three callbacks. Reads/skips fail (return false) rather than run past the
 * end; the write callback returns 0 (cmp treats that as failure) when the
 * destination buffer is full. */

static bool msgpack_reader_cb(cmp_ctx_t *ctx, void *data, size_t limit)
{
    struct picomesh_msgpack_buffer *buf = ctx->buf;
    if (limit > buf->cap || buf->offset > buf->cap - limit)
        return false;
    memcpy(data, buf->data + buf->offset, limit);
    buf->offset += limit;
    return true;
}

static bool msgpack_skipper_cb(cmp_ctx_t *ctx, size_t count)
{
    struct picomesh_msgpack_buffer *buf = ctx->buf;
    if (count > buf->cap || buf->offset > buf->cap - count)
        return false;
    buf->offset += count;
    return true;
}

static size_t msgpack_writer_cb(cmp_ctx_t *ctx, const void *data, size_t count)
{
    struct picomesh_msgpack_buffer *buf = ctx->buf;
    if (count > buf->cap || buf->offset > buf->cap - count)
        return 0;
    memcpy(buf->data + buf->offset, data, count);
    buf->offset += count;
    return count;
}

void picomesh_msgpack_reader_init(cmp_ctx_t *cmp, struct picomesh_msgpack_buffer *buf,
                                  const void *data, size_t len)
{
    buf->data = (uint8_t *)(uintptr_t)data; /* read-only use */
    buf->cap = len;
    buf->offset = 0;
    cmp_init(cmp, buf, msgpack_reader_cb, msgpack_skipper_cb, NULL);
}

void picomesh_msgpack_writer_init(cmp_ctx_t *cmp, struct picomesh_msgpack_buffer *buf,
                                  void *data, size_t cap)
{
    buf->data = data;
    buf->cap = cap;
    buf->offset = 0;
    cmp_init(cmp, buf, NULL, NULL, msgpack_writer_cb);
}
