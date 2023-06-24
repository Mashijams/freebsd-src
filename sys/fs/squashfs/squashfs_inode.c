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