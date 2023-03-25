/*
 * FastRPC operating system interface implementation
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aee_error.h"
#include "interfaces/apps_std.def"
#include "hexagonfs.h"
#include "iobuffer.h"
#include "listener.h"

struct apps_std_fread_invoke {
	uint32_t fd;
	uint32_t buf_size;
};

struct apps_std_fread_return {
	uint32_t written;
	uint32_t is_eof;
};



static int apps_std_whence_table[] = {
	SEEK_SET,
	SEEK_CUR,
	SEEK_END,
};

static int rootfd = -EBADF;
static int adsp_avs_cfg_dirfd = -EBADF;
static int adsp_library_dirfd = -EBADF;

static int open_dirs()
{
	if (rootfd < 0) {
		rootfd = hexagonfs_open_root();
		if (rootfd < 0)
			return rootfd;
	}

	if (adsp_avs_cfg_dirfd < 0) {
		adsp_avs_cfg_dirfd = hexagonfs_openat(rootfd, rootfd,
						      "/usr/lib/qcom/adsp/avs/");
		if (adsp_avs_cfg_dirfd < 0)
			return adsp_avs_cfg_dirfd;
	}

	if (adsp_library_dirfd < 0) {
		adsp_library_dirfd = hexagonfs_openat(rootfd, rootfd,
						      "/usr/lib/qcom/adsp/");
		if (adsp_library_dirfd < 0)
			return adsp_library_dirfd;
	}

	return 0;
}

/*
 * This is a placeholder function used to complete any I/O operations.
 * File descriptors do not have a flush operation because their reads and
 * writes are blocking.
 */
static uint32_t apps_std_fflush(const struct fastrpc_io_buffer *inbufs,
				struct fastrpc_io_buffer *outbufs)
{
	uint32_t *fd = inbufs[0].p;

#ifdef HEXAGONRPC_VERBOSE
	printf("ignore fflush(%u)\n", *fd);
#endif

	memset(outbufs[0].p, 0, outbufs[0].s);

	return 0;
}

static uint32_t apps_std_fclose(const struct fastrpc_io_buffer *inbufs,
				struct fastrpc_io_buffer *outbufs)
{
	const uint32_t *first_in = inbufs[0].p;
	int ret;

	ret = hexagonfs_close(*first_in);
	if (ret) {
		fprintf(stderr, "Could not close: %s\n", strerror(-ret));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("close(%u)\n", *first_in);
#endif

	return 0;
}

static uint32_t apps_std_fread(const struct fastrpc_io_buffer *inbufs,
			       struct fastrpc_io_buffer *outbufs)
{
	const struct apps_std_fread_invoke *first_in = inbufs[0].p;
	struct apps_std_fread_return *first_out = outbufs[0].p;
	ssize_t ret;

	ret = hexagonfs_read(first_in->fd, first_in->buf_size, outbufs[1].p);
	if (ret < 0) {
		fprintf(stderr, "Could not read file: %s\n", strerror(-ret));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("read(%u, %u) -> %ld\n", first_in->fd,
					first_in->buf_size,
					ret);
#endif

	first_out->written = ret;
	first_out->is_eof = first_out->written < first_in->buf_size;

	return 0;
}

static uint32_t apps_std_fseek(const struct fastrpc_io_buffer *inbufs,
			       struct fastrpc_io_buffer *outbufs)
{
	struct {
		uint32_t fd;
		uint32_t pos;
		uint32_t whence;
	} *first_in = inbufs[0].p;
	int ret;
	int whence;

	whence = apps_std_whence_table[first_in->whence];

	ret = hexagonfs_lseek(first_in->fd, first_in->pos, whence);
	if (ret) {
		fprintf(stderr, "Could not seek stream: %s\n", strerror(-ret));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("lseek(%u, %d, %d)\n", first_in->fd,
				      first_in->pos,
				      first_in->whence);
#endif

	return 0;
}

static uint32_t apps_std_fopen_with_env(const struct fastrpc_io_buffer *inbufs,
					struct fastrpc_io_buffer *outbufs)
{
	const struct {
		uint32_t envvarname_len;
		uint32_t delim_len;
		uint32_t name_len;
		uint32_t mode_len;
	} *first_in = inbufs[0].p;
	uint32_t *out = outbufs[0].p;
	char rw_mode;
	int ret, dirfd, fd;

	// The name and environment variable must also be NULL-terminated
	if (((const char *) inbufs[1].p)[first_in->envvarname_len - 1] != 0
	 || ((const char *) inbufs[3].p)[first_in->name_len - 1] != 0
	 || ((const char *) inbufs[4].p)[first_in->mode_len - 1] != 0)
		return AEE_EBADPARM;

	rw_mode = ((const char *) inbufs[4].p)[0];
	if (rw_mode == 'w' || rw_mode == 'a') {
		fprintf(stderr, "Tried to open %s for writing\n",
				(const char *) inbufs[3].p);
		return AEE_EUNSUPPORTED;
	}

	ret = open_dirs();
	if (ret) {
		fprintf(stderr, "Could not open directories: %s\n",
				strerror(-ret));
		return AEE_EFAILED;
	}

	if (!strcmp(inbufs[1].p, "ADSP_LIBRARY_PATH")) {
		dirfd = adsp_library_dirfd;
	} else if (!strcmp(inbufs[1].p, "ADSP_AVS_CFG_PATH")) {
		dirfd = adsp_avs_cfg_dirfd;
	} else {
		fprintf(stderr, "Unknown search directory %s\n",
				(const char *) inbufs[1].p);
		return AEE_EBADPARM;
	}

	fd = hexagonfs_openat(rootfd, dirfd, inbufs[3].p);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s: %s\n",
				(const char *) inbufs[3].p,
				strerror(errno));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("openat($%s, %s) -> %d\n", (const char *) inbufs[1].p,
					  (const char *) inbufs[3].p,
					  fd);
#endif

	*out = fd;

	return 0;
}

static uint32_t apps_std_opendir(const struct fastrpc_io_buffer *inbufs,
				 struct fastrpc_io_buffer *outbufs)
{
	const uint32_t *namelen = inbufs[0].p;
	uint64_t *dir_out = outbufs[0].p;
	int ret;

	// The name must be NULL-terminated
	if (((const char *) inbufs[1].p)[*namelen - 1] != 0)
		return AEE_EBADPARM;

	ret = open_dirs();
	if (ret) {
		fprintf(stderr, "Could not open directories: %s\n",
				strerror(-ret));
		return AEE_EFAILED;
	}

	ret = hexagonfs_openat(rootfd, rootfd, inbufs[1].p);
	if (ret < 0) {
		fprintf(stderr, "Could not open %s: %s\n",
				(const char *) inbufs[1].p,
				strerror(-ret));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("opendir(%s) -> %d\n", (const char *) inbufs[1].p, ret);
#endif

	*dir_out = ret;

	return 0;
}

static uint32_t apps_std_closedir(const struct fastrpc_io_buffer *inbufs,
				  struct fastrpc_io_buffer *outbufs)
{
	const uint64_t *dir = inbufs[0].p;
	int ret;

	ret = hexagonfs_close(*dir);
	if (ret)
		return AEE_EFAILED;

#ifdef HEXAGONRPC_VERBOSE
	printf("closedir(%ld)\n", *dir);
#endif

	return 0;
}

static uint32_t apps_std_readdir(const struct fastrpc_io_buffer *inbufs,
				 struct fastrpc_io_buffer *outbufs)
{
	const uint64_t *dir = inbufs[0].p;
	struct {
		uint32_t inode;
		char name[255];
		uint32_t is_eof;
	} *first_out = outbufs[0].p;
	int ret;

	ret = hexagonfs_readdir(*dir, 255, first_out->name);
	if (ret < 0) {
		fprintf(stderr, "Could not read from directory: %s\n",
				strerror(-ret));
		return AEE_EFAILED;
	}

#ifdef HEXAGONRPC_VERBOSE
	printf("readdir(%ld) -> %s\n", *dir, first_out->name);
#endif

	first_out->inode = 0;
	first_out->is_eof = (*first_out->name == '\0');

	return 0;
}

static uint32_t apps_std_stat(const struct fastrpc_io_buffer *inbufs,
			      struct fastrpc_io_buffer *outbufs)
{
	char *pathname;
	const struct {
		uint32_t method;
		uint32_t pathname_len;
	} *first_in = inbufs[0].p;
	struct {
		uint64_t tsz; // Unknown purpose
		uint64_t dev;
		uint64_t ino;
		uint32_t mode;
		uint32_t nlink;
		uint64_t rdev;
		uint64_t size;
		int64_t atime;
		int64_t atimensec;
		int64_t mtime;
		int64_t mtimensec;
		int64_t ctime;
		int64_t ctimensec;
	} *first_out = outbufs[0].p;
	struct stat stats;
	int fd, ret;

	pathname = malloc(first_in->pathname_len + 1);
	memcpy(pathname, inbufs[1].p, first_in->pathname_len);
	pathname[first_in->pathname_len] = 0;

	ret = open_dirs();
	if (ret) {
		fprintf(stderr, "Could not open directories: %s\n",
				strerror(-ret));
		goto err_free_pathname;
	}

	fd = hexagonfs_openat(rootfd, rootfd, pathname);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s: %s\n",
				pathname, strerror(-fd));
		goto err_free_pathname;
	}

	ret = hexagonfs_fstat(fd, &stats);
	if (ret) {
		fprintf(stderr, "Could not stat %s: %s\n",
				pathname, strerror(-fd));
		goto err_free_pathname;
	}

	hexagonfs_close(fd);

#ifdef HEXAGONRPC_VERBOSE
	printf("stat(%s)\n", pathname);
#endif

	first_out->tsz = 0;

	first_out->dev = stats.st_dev;
	first_out->ino = stats.st_ino;
	first_out->mode = stats.st_mode;
	first_out->nlink = stats.st_nlink;
	first_out->rdev = stats.st_rdev;
	first_out->size = stats.st_size;
	first_out->atime = (int64_t) stats.st_atim.tv_sec;
	first_out->atimensec = (int64_t) stats.st_atim.tv_nsec;
	first_out->mtime = (int64_t) stats.st_mtim.tv_sec;
	first_out->mtimensec = (int64_t) stats.st_mtim.tv_nsec;
	first_out->ctime = (int64_t) stats.st_ctim.tv_nsec;
	first_out->ctimensec = (int64_t) stats.st_ctim.tv_nsec;

	free(pathname);

	return 0;

err_free_pathname:
	free(pathname);
	return AEE_EFAILED;
}

const struct fastrpc_interface apps_std_interface = {
	.name = "apps_std",
	.n_procs = 32,
	.procs = {
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{
			.def = &apps_std_fflush_def,
			.impl = apps_std_fflush,
		},
		{
			.def = &apps_std_fclose_def,
			.impl = apps_std_fclose,
		},
		{
			.def = &apps_std_fread_def,
			.impl = apps_std_fread,
		},
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{
			.def = &apps_std_fseek_def,
			.impl = apps_std_fseek,
		},
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{
			.def = &apps_std_fopen_with_env_def,
			.impl = apps_std_fopen_with_env,
		},
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{
			.def = &apps_std_opendir_def,
			.impl = apps_std_opendir,
		},
		{
			.def = &apps_std_closedir_def,
			.impl = apps_std_closedir,
		},
		{
			.def = &apps_std_readdir_def,
			.impl = apps_std_readdir,
		},
		{ .def = NULL, .impl = NULL, },
		{ .def = NULL, .impl = NULL, },
		{
			.def = &apps_std_stat_def,
			.impl = apps_std_stat,
		},
	},
};
