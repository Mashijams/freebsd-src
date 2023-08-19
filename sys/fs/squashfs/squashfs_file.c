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

static	MALLOC_DEFINE(M_SQSHBLKIDX, "Sqsh Blk idx", "Squashfs block index");

void	swapendian_fragment_entry(struct sqsh_fragment_entry *temp);

size_t
sqsh_blocklist_count(struct sqsh_mount *ump, struct sqsh_inode *inode)
{
	uint64_t size = inode->size;
	size_t block = ump->sb.block_size;
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

sqsh_err
sqsh_blocklist_next(struct sqsh_blocklist *bl)
{
	sqsh_err err;
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
		bl->pos += bl->ump->sb.block_size;
	bl->started = true;

	return SQFS_OK;
}

static bool
sqsh_blockidx_indexable(struct sqsh_mount *ump, struct sqsh_inode *inode)
{
	size_t blocks = sqsh_blocklist_count(ump, inode);
	size_t md_size = blocks * sizeof(uint32_t);
	return md_size >= SQUASHFS_METADATA_SIZE;
}

sqsh_err
sqsh_blockidx_add(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_blockidx_entry **out)
{
	size_t blocks;
	size_t md_size;
	size_t count;

	struct sqsh_blockidx_entry *blockidx;
	struct sqsh_blocklist bl;

	size_t i;
	bool first;

	i = 0;
	first = true;
	*out = NULL;

	blocks = sqsh_blocklist_count(ump, inode);
	md_size = blocks * sizeof(uint32_t);
	count = (inode->next.offset + md_size - 1)
		/ SQUASHFS_METADATA_SIZE;
	blockidx = malloc(count * sizeof(struct sqsh_blockidx_entry), M_SQSHBLKIDX,
	    M_WAITOK | M_ZERO);
	if (blockidx == NULL)
		return SQFS_ERR;

	sqsh_blocklist_init(ump, inode, &bl);
	while (bl.remain && i < count) {
		sqsh_err err;
		/* skip the first metadata block since its stored in inode */
		if (bl.cur.offset < sizeof(uint32_t) && !first) {
			blockidx[i].data_block = bl.block + bl.input_size;
			blockidx[i++].md_block = (uint32_t)(bl.cur.block - ump->sb.inode_table_start);
		}
		first = false;

		err = sqsh_blocklist_next(&bl);
		if (err != SQFS_OK) {
			free(blockidx, M_SQSHBLKIDX);
			return SQFS_ERR;
		}
	}

	*out = blockidx;
	return SQFS_OK;
}

sqsh_err
sqsh_blockidx_blocklist(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_blocklist *bl, off_t start)
{
	size_t block, metablock, skipped;
	struct sqsh_blockidx_entry *blockidx;
	sqsh_err err;

	sqsh_blocklist_init(ump, inode, bl);
	block = (size_t)(start / ump->sb.block_size);
	/* fragment */
	if (block > bl->remain) {
		bl->remain = 0;
		return SQFS_OK;
	}

	/* Total blocks we need to skip */
	metablock = (bl->cur.offset + block * sizeof(uint32_t))
		/ SQUASHFS_METADATA_SIZE;
	if (metablock == 0)
		return SQFS_OK;
	if (!sqsh_blockidx_indexable(ump, inode))
		return SQFS_OK;

	err = sqsh_blockidx_add(ump, inode, &blockidx);
	if (err != SQFS_OK) {
		return err;
	}

	skipped = (metablock * SQUASHFS_METADATA_SIZE / sizeof(uint32_t))
		- (bl->cur.offset / sizeof(uint32_t));

	blockidx += metablock - 1;
	bl->cur.block = blockidx->md_block + ump->sb.inode_table_start;
	bl->cur.offset %= sizeof(uint32_t);
	bl->remain -= skipped;
	bl->pos = (uint64_t)skipped * ump->sb.block_size;
	bl->block = blockidx->data_block;

	/* free blockidx */
	free(blockidx, M_SQSHBLKIDX);

	return SQFS_OK;
}

sqsh_err
sqsh_frag_entry(struct sqsh_mount *ump, struct sqsh_fragment_entry *frag,
	uint32_t idx)
{
	sqsh_err err;

	if (idx == SQUASHFS_INVALID_FRAG)
		return SQFS_ERR;

	err = sqsh_get_table(&ump->frag_table, ump, idx, frag);
	swapendian_fragment_entry(frag);
	return err;
}

sqsh_err
sqsh_frag_block(struct sqsh_mount *ump, struct sqsh_inode *inode,
	size_t *offset, size_t *size, struct sqsh_block **block)
{
	struct sqsh_fragment_entry frag;
	sqsh_err err;

	if (inode->type != VREG)
		return SQFS_ERR;

	err = sqsh_frag_entry(ump, &frag, inode->xtra.reg.frag_idx);
	if (err != SQFS_OK)
		return err;

	err = sqsh_data_read(ump, frag.start_block, frag.size, block);
	if (err != SQFS_OK)
		return SQFS_ERR;

	*offset = inode->xtra.reg.frag_off;
	*size = inode->size % ump->sb.block_size;

	return (err);
}

sqsh_err
sqsh_read_file(struct sqsh_mount *ump, struct sqsh_inode *inode,
	off_t start, off_t *size, void *buf)
{
	sqsh_err err;
	off_t file_size;
	size_t block_size;
	struct sqsh_blocklist bl;
	size_t read_off;
	char *buf_orig;

	file_size = inode->size;
	block_size = ump->sb.block_size;

	if (*size < 0 || start > file_size)
		return SQFS_ERR;
	if (start == file_size) {
		*size = 0;
		return SQFS_OK;
	}

	err = sqsh_blockidx_blocklist(ump, inode, &bl, start);
	if (err != SQFS_OK)
		return err;

	read_off = start % block_size;
	buf_orig = buf;
	while (*size > 0) {
		struct sqsh_block *block;
		size_t data_off, data_size;
		size_t take;
		bool fragment;

		block = NULL;
		fragment = (bl.remain == 0);

		if (fragment) {
			if (inode->xtra.reg.frag_idx == SQUASHFS_INVALID_FRAG)
				break;
			err = sqsh_frag_block(ump, inode, &data_off, &data_size, &block);
			if (err != SQFS_OK)
				return err;
		} else {
			err = sqsh_blocklist_next(&bl);
			if (err != SQFS_OK)
				return err;
			if (bl.pos + block_size <= start)
				continue;

			data_off = 0;
			if (bl.input_size == 0) {
				data_size = (size_t)(file_size - bl.pos);
				if (data_size > block_size)
					data_size = block_size;
			} else {
				err = sqsh_data_read(ump, bl.block, bl.header, &block);
				if (err != SQFS_OK)
					return err;
				data_size = block->size;
			}
		}

		take = data_size - read_off;
		if (take > *size)
			take = (size_t)(*size);
		if (block != NULL) {
			memcpy(buf, (char*)block->data + data_off + read_off, take);
			/* free the allocated block since we have no cache now */
			sqsh_free_block(block);
		} else {
			memset(buf, 0, take);
		}
		read_off = 0;
		*size -= take;
		buf = (char*)buf + take;

		if (fragment)
			break;
	}

	off_t data_read;
	data_read = (char*)buf - buf_orig;
	buf = buf_orig;
	return data_read ? SQFS_OK : SQFS_ERR;
}

void
swapendian_fragment_entry(struct sqsh_fragment_entry *temp)
{
	temp->start_block	=	le64toh(temp->start_block);
	temp->size			=	le32toh(temp->size);
	temp->unused		=	le32toh(temp->unused);
}
