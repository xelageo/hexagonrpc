#ifndef IOBUFFER_H
#define IOBUFFER_H

#include <stddef.h>
#include <sys/types.h>

struct fastrpc_io_buffer {
	size_t s;
	void *p;
};

struct fastrpc_decoder_context {
	struct fastrpc_io_buffer *inbufs;
	int idx;
	size_t size;
	int n_inbufs;

	off_t size_off;
	off_t buf_off;
	off_t align;
};

void iobuf_free(size_t n_iobufs, struct fastrpc_io_buffer *iobufs);

struct fastrpc_decoder_context *inbuf_decode_start(uint32_t sc);
struct fastrpc_io_buffer *inbuf_decode_finish(struct fastrpc_decoder_context *ctx);
void inbuf_decode(struct fastrpc_decoder_context *ctx, size_t len, const void *src);

#endif
