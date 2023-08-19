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

#ifndef SQUASHFS_FILE_H
#define SQUASHFS_FILE_H

struct sqsh_blocklist {
	struct sqsh_mount		*ump;
	size_t					remain;
	struct sqsh_block_run	cur;
	bool					started;
	uint64_t				pos;
	uint64_t				block;
	uint32_t				header;
	uint32_t				input_size;
};

struct sqsh_blockidx_entry {
	uint64_t	data_block;
	uint32_t	md_block;
};

/* sqsh_blocklist helper functions */
size_t		sqsh_blocklist_count(struct sqsh_mount *ump, struct sqsh_inode *inode);
void		sqsh_blocklist_init(struct sqsh_mount *ump, struct sqsh_inode *inode,
				struct sqsh_blocklist *bl);
sqsh_err	sqsh_blocklist_next(struct sqsh_blocklist *bl);

/* Block index helper functions for skipping to the middle of large files */
static bool	sqsh_blockidx_indexable(struct sqsh_mount *ump, struct sqsh_inode *inode);
sqsh_err	sqsh_blockidx_add(struct sqsh_mount *ump, struct sqsh_inode *inode,
				struct sqsh_blockidx_entry **out);
sqsh_err	sqsh_blockidx_blocklist(struct sqsh_mount *ump, struct sqsh_inode *inode,
				struct sqsh_blocklist *bl, off_t start);

/* fragments helper functions */
sqsh_err	sqsh_frag_entry(struct sqsh_mount *ump, struct sqsh_fragment_entry *frag,
				uint32_t idx);
sqsh_err	sqsh_frag_block(struct sqsh_mount *ump, struct sqsh_inode *inode,
				size_t *offset, size_t *size, struct sqsh_block **block);

sqsh_err	sqsh_read_file(struct sqsh_mount *ump, struct sqsh_inode *inode,
				off_t start, off_t *size, void *buf);

#endif