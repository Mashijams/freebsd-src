/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Raghav Sharma <raghav@freebsd.org>
 * Parts Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * Obtained from the squashfuse project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef SQUASHFS_DIR_H
#define SQUASHFS_DIR_H

struct sqsh_dir_entry {
	uint64_t	inode_id;
	uint32_t	inode_number;
	char		name[100];
	size_t		name_size;
	off_t		offset;
	off_t		next_offset;
};

/* Helper for sqsh_dir_lookup */
struct sqsh_dir_ff_name_t {
	const char	*cmp;
	size_t		cmplen;
	char		*name;
};

/* Initialise directory from inode */
sqsh_err	sqsh_dir_init(struct sqsh_mount *ump, struct sqsh_inode *inode,
				struct sqsh_dir *dir);

/* Directory indexing helper functions */
sqsh_err	sqsh_dir_f_header(struct sqsh_mount *ump, struct sqsh_block_run *cur,
				struct sqsh_dir_index *idx, bool *stop, void *arg);
sqsh_err	sqsh_dir_ff_header(struct sqsh_mount *ump, struct sqsh_inode *inode,
				struct sqsh_dir *dir, void *arg);

sqsh_err	sqsh_dir_metadata_read(struct sqsh_mount *mnt, struct sqsh_dir *dir,
				void *buf, size_t size);

/* Directory traverse helper functions for vnops readdir and lookup */
sqsh_err	sqsh_dir_getnext(struct sqsh_mount *ump, struct sqsh_dir *dir,
				struct sqsh_dir_entry *entry);
sqsh_err	sqsh_dir_lookup(struct sqsh_mount *ump, struct sqsh_inode *inode,
				const char *name, size_t namelen, struct sqsh_dir_entry *entry,
				bool *found);

#endif /* SQUASHFS_DIR_H */