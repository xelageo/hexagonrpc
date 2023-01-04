/*
 * FastRPC Example
 *
 * Copyright (C) 2022 Richard Acayan
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

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "fastrpc.h"
#include "fastrpc_remotectl.h"

static int remotectl_open(int fd, char *name, struct fastrpc_context **ctx, void (*err_cb)(const char *err, size_t err_len))
{
	uint32_t handle;
	uint32_t err_len;
	char err[256];
	int ret;

	ret = fastrpc2(&remotectl_open_def, fd, REMOTECTL_HANDLE,
		       strlen(name) + 1, name,
		       &handle,
		       256, &err_len, err);
	if (ret) {
		err_cb(err, err_len);
		return ret;
	}

	*ctx = fastrpc_create_context(fd, handle);

	return 0;
}

static void remotectl_err(const char *err, size_t err_len)
{
	write(STDERR_FILENO, err, err_len);
}

int main()
{
	struct fastrpc_context *ctx;

	remotectl_open(-1, "adsp_listener", &ctx, remotectl_err);
	printf("{ .fd = %u, .handle = %u, }\n", ctx->fd, ctx->handle);
	free(ctx);
}
