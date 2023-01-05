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

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "aee_error.h"
#include "fastrpc.h"
#include "fastrpc_remotectl.h"

static int remotectl_open(int fd, char *name, struct fastrpc_context **ctx, void (*err_cb)(const char *err))
{
	uint32_t handle;
	uint32_t dlret;
	char err[256];
	int ret;

	ret = fastrpc2(&remotectl_open_def, fd, REMOTECTL_HANDLE,
		       strlen(name) + 1, name,
		       &handle,
		       &dlret,
		       256, err);

	if (ret == -1) {
		err_cb(strerror(errno));
		return ret;
	}

	if (dlret) {
		err_cb(aee_strerror[dlret]);
		return dlret;
	}

	*ctx = fastrpc_create_context(fd, handle);

	return ret;
}

static void remotectl_err(const char *err)
{
	fprintf(stderr, "Could not remotectl: %s\n", err);
}

int main()
{
	struct fastrpc_context *ctx;

	remotectl_open(-1, "adsp_default_listener", &ctx, remotectl_err);
	printf("{ .fd = %u, .handle = %u, }\n", ctx->fd, ctx->handle);
	free(ctx);
}
