/*
 * Remote processor control interface
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

#ifndef FASTRPC_REMOTECTL_H
#define FASTRPC_REMOTECTL_H

#include "fastrpc.h"

#define REMOTECTL_HANDLE 0

#define DEFINE_REMOTE_PROCEDURE(mid, name, innums, inbufs, outnums, outbufs)	\
	struct fastrpc_function_def_interp2 name##_def = {			\
		.msg_id = mid,							\
		.in_nums = innums,						\
		.in_bufs = inbufs,						\
		.out_nums = outnums,						\
		.out_bufs = outbufs,						\
	}

DEFINE_REMOTE_PROCEDURE(0, remotectl_open, 0, 1, 2, 1);
DEFINE_REMOTE_PROCEDURE(0, remotectl_close, 1, 0, 1, 1);

#undef DEFINE_REMOTE_PROCEDURE

#endif
