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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <squashfs.h>
#include <squashfs_io.h>
#include <squashfs_mount.h>
#include <squashfs_inode.h>
#include <squashfs_block.h>
#include <squashfs_file.h>

size_t
sqsh_blocklist_count(struct sqsh_mount *ump, struct sqsh_inode *inode)
{
	uint64_t size = inode->xtra.reg.file_size;
	size_t block = fs->sb.block_size;
	if (inode->xtra.reg.frag_idx == SQUASHFS_INVALID_FRAG) {
		return sqsh_ceil(size, block);
	} else {
		return (size_t)(size / block);
	}
}

void
sqsh_blocklist_init(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_blocklist *bl)
{
	bl->ump			=	ump;
	bl->remain		=	sqsh_blocklist_count(ump, inode);
	bl->cur			=	inode->next;
	bl->started		=	false;
	bl->pos			=	0;
	bl->block		=	inode->xtra.reg.start_block;
	bl->input_size	=	0;
}

sqfs_err
sqsh_blocklist_next(struct sqsh_blocklist *bl)
{
	sqfs_err err;
	bool compressed;

	err = SQFS_OK;

	if (bl->remain == 0)
		return SQFS_ERR;
	--(bl->remain);

	err = sqsh_metadata_get(bl->ump, &bl->cur, &bl->header, sizeof(bl->header));
	if (err != SQFS_OK)
		return err;
	bl->header = le32toh(bl->header);
	bl->block += bl->input_size;
	sqsh_data_header(bl->header, &compressed, &bl->input_size);

	if (bl->started)
		bl->pos += bl->fs->sb.block_size;
	bl->started = true;

	return SQFS_OK;
}

static bool
sqsh_blockidx_indexable(struct sqsh_mount *ump, struct sqsh_inode *inode)
{
	size_t blocks = sqsh_blocklist_count(ump, inode);
	size_t md_size = blocks * sizeof(sqsh_blocklist_entry);
	return md_size >= SQUASHFS_METADATA_SIZE;
}
