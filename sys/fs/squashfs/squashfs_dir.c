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
#include <squashfs_dir.h>

void	swapendian_dir_header(struct sqsh_dir_header *hdr);
void	swapendian_dir_index(struct sqsh_dir_index *idx);

sqsh_err
sqsh_dir_init(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_dir *dir)
{
	memset(dir, 0, sizeof(*dir));
	dir->cur.block = inode->xtra.dir.start_block +
		ump->sb.directory_table_start;
	dir->cur.offset = inode->xtra.dir.offset;
	dir->offset = 0;

	/*
	 * For better compression '.' and '..' entries
	 * are not there in squashfs but Inode entires
	 * does keep directory count including them.
	 * Here we are just chekcing for that and updating
	 * dir count accordingly.
	 */
	dir->total = inode->xtra.dir.dir_size <= 3 ? 0 :
                inode->xtra.dir.dir_size - 3;

	return SQFS_OK;
}

sqsh_err
sqsh_dir_f_header(struct sqsh_mount *ump, struct sqsh_block_run *cur,
	struct sqsh_dir_index *idx, bool *stop, void *arg)
{
	off_t offset = *(off_t*)arg;

	if (idx->index >= offset) {
		*stop = true;
		return SQFS_OK;
	}

	return sqsh_metadata_get(ump, cur, NULL, index->size + 1);
}

sqsh_err
sqsh_dir_ff_header(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_dir *dir, void *arg)
{
	struct sqsh_dir_index idx;
	sqsh_block_run cur;
	size_t count;

	cur		= inode->next;
	count	= inode->xtra.dir.idx_count;

	if (count == 0)
		return SQFS_OK;

	while (count--) {
		sqsh_err err;
		bool stop;

		stop = false;

		err = sqsh_metadata_get(ump, &cur, &idx, sizeof(idx));

		if (err != SQFS_OK)
			return err;

		swapendian_dir_index(&idx);

		err = sqsh_dir_f_header(ump, &cur, &idx, &stop, arg);

		if (err != SQFS_OK)
			return err;
		if (stop)
			break;

		dir->cur.block = idx.start_block + ump->sb.directory_table_start;
		dir->offset = idx.index;
	}

	dir->cur.offset = (dir->cur.offset + dir->offset) % SQUASHFS_METADATA_SIZE;
	return SQFS_OK;
}

sqsh_err
sqsh_dir_metadata_read(struct sqsh_mount *ump, struct sqsh_dir *dir, void *buf,
	size_t size)
{
	dir->offset += size;
	return sqsh_metadata_get(ump, &dir->cur, buf, size);
}

sqsh_err
sqsh_dir_lookup(struct sqsh_mount *ump, struct sqsh_inode *inode, const char *name,
	size_t namelen, struct sqsh_dir_entry *entry, bool *found)
{

}

void
swapendian_dir_header(struct sqsh_dir_header *hdr)
{
	hdr->count			=	le32toh(hdr->count);
	hdr->start_block	=	le32toh(hdr->start_block);
	hdr->inode_number	=	le32toh(hdr->inode_number);
}

void
swapendian_dir_index(struct sqsh_dir_index *idx)
{
	idx->index			=	le32toh(idx->index);
	idx->start_block	=	le32toh(idx->start_block);
	idx->size			=	le32toh(idx->size);
}