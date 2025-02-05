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

#ifndef	SQUASHFS_INODE_H
#define	SQUASHFS_INODE_H

#define	SQUASHFS_INODE_OFFSET(A)	((unsigned int) ((A) & 0xffff))

struct sqsh_inode {
	struct sqsh_base_inode	base;
	int						nlink;
	uint32_t				xattr;
	size_t					size;

	enum vtype				type;

	struct sqsh_block_run	next;

	union {
		struct {
			int				major;
			int				minor;
		} dev;
		struct {
			uint64_t		start_block;
			uint32_t		frag_idx;
			uint32_t		frag_off;
		} reg;
		struct {
			uint32_t		start_block;
			uint16_t		offset;
			uint16_t		idx_count;
			uint32_t		parent_inode;
		} dir;
	} xtra;

	struct vnode			*vnode;
};

/* helper functions to query on inode */
void		sqsh_metadata_run_inode(struct sqsh_block_run *cur, uint64_t id,
				off_t base);
sqsh_err	sqsh_get_inode(struct sqsh_mount *ump, struct sqsh_inode *inode,
				uint64_t id);

enum vtype	sqsh_inode_type(int inode_type);

uint64_t	sqsh_root_inode(struct sqsh_mount *ump);
sqsh_err	sqsh_verify_inode(struct sqsh_mount *ump, struct sqsh_inode *inode);

#endif