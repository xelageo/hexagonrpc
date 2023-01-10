/*
 * FastRPC reverse tunnel - argument decoder
 *
 * Copyright (C) 2023 Richard Acayan
 *
 * This file is part of sensh.
 *
 * Sensh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fastrpc.h"
#include "iobuffer.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static size_t consume_size(struct fastrpc_decoder_context *ctx,
			   size_t len, const char *buf)
{
	size_t segment;

	segment = MIN(len, 4 - ctx->size_off);
	memcpy(&((char *) &ctx->size)[ctx->size_off], buf, segment);
	ctx->size_off = (ctx->size_off + segment) % 4;
	ctx->align = (ctx->align + segment) & 0x7;

	return segment;
}

static void try_populate_inbuf(struct fastrpc_decoder_context *ctx)
{
	if (!ctx->size_off) {
		ctx->inbufs[ctx->idx].s = ctx->size;
		ctx->inbufs[ctx->idx].p = malloc(ctx->size);
	}
}

static size_t consume_alignment(struct fastrpc_decoder_context *ctx, size_t len)
{
	size_t segment = 0;

	if (ctx->align) {
		segment = MIN(len, 8 - ctx->align);
		ctx->align = (ctx->align + segment) & 0x7;
	}

	return segment;
}

static size_t consume_buf(struct fastrpc_decoder_context *ctx,
			  size_t len, const void *buf)
{
	size_t segment;
	void *dest;

	dest = (void *) &((char *) ctx->inbufs[ctx->idx].p)[ctx->buf_off];

	segment = MIN(len, ctx->size - ctx->buf_off);
	memcpy(dest, buf, segment);

	if (ctx->buf_off + segment >= ctx->size) {
		ctx->size = 0;
		ctx->buf_off = 0;
		ctx->idx++;
	} else {
		ctx->buf_off = ctx->buf_off + segment;
	}

	ctx->align = (ctx->align + segment) & 0x7;

	return segment;
}

void iobuf_free(size_t n_iobufs, struct fastrpc_io_buffer *iobufs)
{
	size_t i;

	for (i = 0; i < n_iobufs; i++)
		free(iobufs[i].p);

	free(iobufs);
}

struct fastrpc_decoder_context *inbuf_decode_start(uint32_t sc)
{
	struct fastrpc_decoder_context *ctx;
	int n_inbufs;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return ctx;

	ctx->idx = 0;
	ctx->size = 0;
	ctx->n_inbufs = REMOTE_SCALARS_INBUFS(sc);

	ctx->size_off = 0;
	ctx->buf_off = 0;
	ctx->align = 0;

	ctx->inbufs = malloc(sizeof(*ctx->inbufs) * ctx->n_inbufs);
	if (ctx->inbufs == NULL)
		goto err;

	return ctx;

err:
	free(ctx);
	return NULL;
}

struct fastrpc_io_buffer *inbuf_decode_finish(struct fastrpc_decoder_context *ctx)
{
	struct fastrpc_io_buffer *inbufs = ctx->inbufs;

	free(ctx);

	return inbufs;
}

void inbuf_decode(struct fastrpc_decoder_context *ctx, size_t len, const void *src)
{
	const char *buf = src;
	size_t off = 0;

	while (off < len && ctx->idx < ctx->n_inbufs) {
		if (!ctx->size || ctx->size_off) {
			off += consume_size(ctx, len - off, &buf[off]);
			try_populate_inbuf(ctx);
		} else {
			off += consume_alignment(ctx, len - off);
			off += consume_buf(ctx, len - off, &buf[off]);
		}
	}
}
