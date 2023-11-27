/*
 * HexagonFS virtual directory operations
 *
 * Copyright (C) 2023 The Sensor Shell Contributors
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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hexagonfs.h"

/*
 * This function searches for the relevant path segment in the path directory.
 * It is not optimized because it is dealing with small directories.
 */
static struct hexagonfs_dirent *walk_dir(struct hexagonfs_dirent **dir,
					 const char *segment)
{
	struct hexagonfs_dirent **curr = dir;

	while (*curr != NULL) {
		if (!strcmp(segment, (*curr)->name))
			break;

		curr++;
	}

	return *curr;
}

static int virt_dir_from_dirent(void *dirent_data, bool dir, void **fd_data)
{
	*fd_data = dirent_data;
	return 0;
}

static int virt_dir_openat(struct hexagonfs_fd *dir,
			   const char *segment,
			   bool expect_dir,
			   struct hexagonfs_fd **out)
{
	struct hexagonfs_fd *fd;
	struct hexagonfs_dirent *ent;
	struct hexagonfs_dirent **dirlist = dir->data;
	int ret;

	ent = walk_dir(dirlist, segment);
	if (ent == NULL)
		return -ENOENT;

	fd = malloc(sizeof(struct hexagonfs_fd));
	if (fd == NULL)
		return -ENOMEM;

	fd->up = dir;
	fd->ops = ent->ops;

	ret = ent->ops->from_dirent(ent->u.ptr, expect_dir, &fd->data);
	if (ret)
		goto err;

	*out = fd;

	return 0;

err:
	free(fd);
	return ret;
}

static void virt_dir_close(void *fd_data)
{
}

static int virt_dir_stat(struct hexagonfs_fd *fd, struct stat *stats)
{
	stats->st_size = 0;

	stats->st_dev = 0;
	stats->st_rdev = 0;

	stats->st_ino = 0;
	stats->st_nlink = 0;

	stats->st_mode = S_IRUSR | S_IXUSR
		       | S_IRGRP | S_IXGRP
		       | S_IROTH | S_IXOTH;

	stats->st_atim.tv_sec = 0;
	stats->st_atim.tv_nsec = 0;
	stats->st_ctim.tv_sec = 0;
	stats->st_ctim.tv_nsec = 0;
	stats->st_mtim.tv_sec = 0;
	stats->st_mtim.tv_nsec = 0;

	return 0;
}

struct hexagonfs_file_ops hexagonfs_virt_dir_ops = {
	.close = virt_dir_close,
	.from_dirent = virt_dir_from_dirent,
	.openat = virt_dir_openat,
	.stat = virt_dir_stat,
};
