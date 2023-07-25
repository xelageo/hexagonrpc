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
#include <libhexagonrpc/fastrpc.h>
#include <misc/fastrpc.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "aee_error.h"
#include "apps_std.h"
#include "hexagonfs.h"
#include "interfaces/adsp_default_listener.def"
#include "interfaces/chre_slpi.def"
#include "interfaces/remotectl.def"
#include "listener.h"
#include "localctl.h"
#include "rpcd_builder.h"

struct listener_thread_args {
	int fd;
	char *device_dir;
	char *dsp;
};

static int remotectl_open(int fd, char *name, struct fastrpc_context **ctx, void (*err_cb)(const char *err))
{
	uint32_t handle;
	int32_t dlret;
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

	if (dlret == -5) {
		err_cb(err);
		return dlret;
	} else if (dlret) {
		err_cb(aee_strerror[dlret]);
		return dlret;
	}

	*ctx = fastrpc_create_context(fd, handle);

	return ret;
}

static int remotectl_close(struct fastrpc_context *ctx, void (*err_cb)(const char *err))
{
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

static int chre_slpi_wait_on_thread_exit(struct fastrpc_context *ctx)
{
	return fastrpc(&chre_slpi_wait_on_thread_exit_def, ctx);
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

static void *start_reverse_tunnel(void *data)
{
	struct listener_thread_args *args = data;
	struct fastrpc_interface **ifaces;
	struct hexagonfs_dirent *root_dir;
	size_t n_ifaces = 2;
	int ret;

	ifaces = malloc(sizeof(struct fastrpc_interface) * n_ifaces);
	if (ifaces == NULL)
		return NULL;

	root_dir = construct_root_dir(args->device_dir, args->dsp);

	/*
	 * The apps_remotectl interface patiently waits for this function to
	 * fully populate the ifaces array as long as it receives a pointer to
	 * it.
	 */
	ifaces[REMOTECTL_HANDLE] = fastrpc_localctl_init(n_ifaces, ifaces);

	// Dynamic interfaces with no hardcoded handle
	ifaces[1] = fastrpc_apps_std_init(root_dir);

	ret = register_fastrpc_listener(args->fd);
	if (ret)
		goto err;

	run_fastrpc_listener(args->fd, n_ifaces, ifaces);

	fastrpc_localctl_deinit(ifaces[REMOTECTL_HANDLE]);

	free(ifaces);

	return NULL;

err:
	free(ifaces);

	return NULL;
}

static void *start_chre_client(void *data)
{
	struct fastrpc_context *ctx;
	int *fd = data;
	int ret;

	ret = remotectl_open(*fd, "chre_slpi", &ctx, remotectl_err);
	if (ret)
		return NULL;

	ret = chre_slpi_start_thread(ctx);
	if (ret) {
		fprintf(stderr, "Could not start CHRE\n");
		goto err;
	}

	ret = chre_slpi_wait_on_thread_exit(ctx);
	if (ret) {
		fprintf(stderr, "Could not wait for CHRE thread\n");
		goto err;
	}

err:
	remotectl_close(ctx, remotectl_err);
	return NULL;
}

int main(int argc, char* argv[])
{
	pthread_t chre_thread, listener_thread;
	struct listener_thread_args *listener_args;
	char *fastrpc_node = NULL;
	int fd, ret, opt;
	bool attach_sns = false;

	listener_args = malloc(sizeof(struct listener_thread_args));
	if (listener_args == NULL) {
		perror("Could not create listener arguments");
		goto err_close_dev;
	}

	listener_args->device_dir = "/usr/share/qcom/";
	listener_args->dsp = "adsp";

	while ((opt = getopt(argc, argv, "d:f:R:s")) != -1) {
		switch (opt) {
			case 'd':
				listener_args->dsp = optarg;
				break;
			case 'f':
				fastrpc_node = optarg;
				break;
			case 'R':
				listener_args->device_dir = optarg;
				break;
			case 's':
				attach_sns = true;
				break;
			default:
				printf("Usage: %s [-s] [-f FastRPC node]\n\n", argv[0]);
				printf("Qualcomm Hexagon filesystem daemon\n\n");
				printf("\t-f\tFastRPC node to attach to\n");
				printf("\t-s\tUse INIT_ATTACH_SNS instead of INIT_ATTACH ioctl\n");
				return 1;
		}
	}

	if (!fastrpc_node) {
		fprintf(stderr, "Invalid FastRPC node: %s\n", fastrpc_node);
		return 2;
	}

	printf("Starting %s (%s) on %s\n", argv[0], attach_sns? "INIT_ATTACH_SNS": "INIT_ATTACH", fastrpc_node);

	fd = open(fastrpc_node, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Could not open FastRPC node (%s): %s\n", fastrpc_node, strerror(errno));
		return 3;
	}

	if (attach_sns)
		ret = ioctl(fd, FASTRPC_IOCTL_INIT_ATTACH_SNS, NULL);
	else
		ret = ioctl(fd, FASTRPC_IOCTL_INIT_ATTACH, NULL);
	if (ret) {
		fprintf(stderr, "Could not attach to FastRPC node: %s\n", strerror(errno));
		goto err_close_dev;
	}

	listener_args->fd = fd;

	pthread_create(&listener_thread, NULL, start_reverse_tunnel, listener_args);
	pthread_create(&chre_thread, NULL, start_chre_client, &fd);

	pthread_join(listener_thread, NULL);
	pthread_join(chre_thread, NULL);

	close(fd);

	return 0;

err_close_dev:
	close(fd);
	return 4;
}
