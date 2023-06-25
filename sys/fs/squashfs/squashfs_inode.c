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

#include<squashfs.h>
#include<squashfs_bin.h>
#include<squashfs_mount.h>
#include<squashfs_inode.h>
#include<squashfs_decompressor.h>
#include<squashfs_block.h>

sqsh_err sqsh_init_table(struct sqsh_table *table, struct sqsh_mount *ump,
	off_t start, size_t each, size_t count) {
	size_t i;
	size_t nblocks, bread;

	if (count == 0)
		return SQFS_OK;

	nblocks = sqsh_ceil(each * count, SQUASHFS_METADATA_SIZE);
	bread = nblocks * sizeof(uint64_t);

	table->each = each;
	table->blocks = malloc(bread);
	if (table->blocks == NULL)
		return SQFS_ERR;

	/*
		Currently we use an array of disk to allocate
		structures and verify metadata on read time.
		This is will change to vfs_() operations once driver
		successfully compiles.
    */
	memcpy(table->blocks, sqfs_image + start, bread);

	// SwapEndian data to host
	for (i = 0; i < nblocks; ++i)
		table->blocks[i] = le64toh(table->blocks[i]);

	return SQFS_OK;
}

void sqsh_free_table(struct sqsh_table *table) {
	free(table->blocks);
	table->blocks = NULL;
}

sqsh_err sqsh_get_table(struct sqsh_table *table, struct sqsh_mount *ump,
	size_t idx, void *buf) {
	struct sqsh_block *block;
	size_t pos = idx * table->each;
	size_t bnum = pos / SQUASHFS_METADATA_SIZE;
	size_t off = pos % SQUASHFS_METADATA_SIZE;

	off_t bpos = table->blocks[bnum];
	size_t data_size = 0;
	if (sqsh_metadata_read(ump, bpos, &data_size, &block))
		return SQFS_ERR;

	memcpy(buf, (char*)(block->data) + off, table->each);
	// Free block since currently we have no cache
	sqsh_free_block(block);
	return SQFS_OK;
}

bool sqsh_export_ok(struct sqsh_mount *ump) {
	return ump->sb.lookup_table_start != SQUASHFS_INVALID_BLK;
}

void sqsh_metadata_run_inode(struct sqsh_block_run *cur, uint64_t id, off_t base) {
	cur->block = (id >> 16) + base;
	cur->offset = id & 0xffff;
}

mode_t sqsh_mode(int inode_type) {
	switch (inode_type) {
		case SQUASHFS_DIR_TYPE:
		case SQUASHFS_LDIR_TYPE:
			return S_IFDIR;
		case SQUASHFS_REG_TYPE:
		case SQUASHFS_LREG_TYPE:
			return S_IFREG;
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE:
			return S_IFLNK;
		case SQUASHFS_BLKDEV_TYPE:
		case SQUASHFS_LBLKDEV_TYPE:
			return S_IFBLK;
		case SQUASHFS_CHRDEV_TYPE:
		case SQUASHFS_LCHRDEV_TYPE:
			return S_IFCHR;
		case SQUASHFS_FIFO_TYPE:
		case SQUASHFS_LFIFO_TYPE:
			return S_IFIFO;
		case SQUASHFS_SOCKET_TYPE:
		case SQUASHFS_LSOCKET_TYPE:
			return S_IFSOCK;
	}
	return 0;
}

sqsh_err sqsh_get_inode_id(struct sqsh_mount *ump, uint16_t idx, uint32_t *id) {
	uint32_t rid;
	sqsh_err err = sqsh_get_table(&ump->id_table, ump, idx, &rid);
	if (err != SQFS_OK)
		return err;
	rid = le32toh(rid);
	*id = rid;
	return SQFS_OK;
}

sqsh_err sqsh_export_inode(struct sqsh_mount *ump, uint32_t n, uint64_t *i) {
	uint64_t r;
	if (!sqsh_export_ok(ump))
		return SQFS_ERR;

	sqsh_err err = sqsh_get_table(&ump->export_table, ump, n - 1, &r);
	if (err)
		return err;
	r = le64toh(r);
	*i = r;
	return SQFS_OK;
}

uint64_t sqsh_root_inode(struct sqsh_mount *ump) {
	return ump->sb.root_inode;
}