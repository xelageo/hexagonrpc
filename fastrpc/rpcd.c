/*
 * FastRPC Example
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

#include <errno.h>
#include <fcntl.h>
#include <misc/fastrpc.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "aee_error.h"
#include "fastrpc.h"
#include "fastrpc_adsp_default_listener.h"
#include "fastrpc_chre_slpi.h"
#include "fastrpc_remotectl.h"

int run_fastrpc_listener(int fd);

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

static int remotectl_close(struct fastrpc_context *ctx, void (*err_cb)(const char *err))
{
	uint32_t handle;
	uint32_t dlret;
	char err[256];
	int ret;

	ret = fastrpc2(&remotectl_close_def, ctx->fd, REMOTECTL_HANDLE,
		       ctx->handle,
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

	fastrpc_destroy_context(ctx);

	return ret;
}

static int adsp_default_listener_register(struct fastrpc_context *ctx)
{
	return fastrpc(&adsp_default_listener_register_def, ctx);
}

static int chre_slpi_start_thread(struct fastrpc_context *ctx)
{
	return fastrpc(&chre_slpi_start_thread_def, ctx);
}

static void remotectl_err(const char *err)
{
	fprintf(stderr, "Could not remotectl: %s\n", err);
}

static int register_fastrpc_listener(int fd)
{
	struct fastrpc_context *ctx;
	int ret;

	ret = remotectl_open(fd, "adsp_default_listener", &ctx, remotectl_err);
	if (ret)
		return 1;

	ret = adsp_default_listener_register(ctx);
	if (ret) {
		fprintf(stderr, "Could not register ADSP default listener\n");
		goto err;
	}

err:
	remotectl_close(ctx, remotectl_err);
	return ret;
}

int main()
{
	struct fastrpc_init_create_static create;
	int fd;
	int ret;

	fd = open("/dev/fastrpc-adsp", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Could not open /dev/fastrpc-adsp: %s\n", strerror(errno));
		return 1;
	}

	ret = ioctl(fd, FASTRPC_IOCTL_INIT_ATTACH_SNS, NULL);
	if (ret) {
		printf("Could not ioctl /dev/fastrpc-adsp: %s\n", strerror(errno));
		goto err_close_dev;
	}

	create.namelen = sizeof("audiopd");
	create.memlen = 1048576;
	create.name = (__u64) "audiopd";
	ret = ioctl(fd, FASTRPC_IOCTL_INIT_CREATE_STATIC, &create);
	if (ret) {
		printf("Could not ioctl /dev/fastrpc-adsp: %s\n", strerror(errno));
		goto err_close_dev;
	}

	ret = register_fastrpc_listener(fd);
	if (ret)
		goto err_close_dev;

	ret = run_fastrpc_listener(fd);
	if (ret)
		goto err_close_dev;

	close(fd);

	return 0;

err_close_dev:
	close(fd);
	return 1;
}
