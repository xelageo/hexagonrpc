/*
 * FastRPC reverse tunnel
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

#include "fastrpc.h"
#include "fastrpc_adsp_listener.h"
#include "iobuffer.h"

static int adsp_listener_init2(int fd)
{
	return fastrpc2(&adsp_listener_init2_def, fd, ADSP_LISTENER_HANDLE);
}

static int adsp_listener_next2(int fd,
			       uint32_t ret_rctx,
			       uint32_t ret_res,
			       uint32_t ret_outbuf_len, void *ret_outbuf,
			       uint32_t *rctx,
			       uint32_t *handle,
			       uint32_t *sc,
			       uint32_t *inbufs_len,
			       uint32_t inbufs_size, void *inbufs)
{
	return fastrpc2(&adsp_listener_next2_def, fd, ADSP_LISTENER_HANDLE,
			ret_rctx,
			ret_res,
			ret_outbuf_len, ret_outbuf,
			rctx,
			handle,
			sc,
			inbufs_len,
			inbufs_size, inbufs);
}

int run_fastrpc_listener(int fd)
{
	struct fastrpc_decoder_context *ctx;
	uint32_t result = 0xffffffff;
	uint32_t handle;
	uint32_t rctx = 0;
	uint32_t sc;
	uint32_t inbufs_len;
	char inbufs[256];
	char outbuf[256];
	int ret;

	ret = adsp_listener_init2(fd);
	if (ret) {
		fprintf(stderr, "Could not initialize the listener: %u\n", ret);
		return ret;
	}

	while (!ret) {
		ret = adsp_listener_next2(fd,
					  rctx, result,
					  0, outbuf,
					  &rctx, &handle, &sc,
					  &inbufs_len, 256, inbufs);
		if (ret)
			break;

		ctx = inbuf_decode_start(sc);
		if (!ctx) {
			ret = -1;
			break;
		}

		inbuf_decode(ctx, inbufs_len, inbufs);

		inbuf_decode_free(ctx);
	}

	return ret;
}
