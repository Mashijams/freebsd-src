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

void sqsh_metadata_header(uint16_t hdr, bool *compressed, uint16_t *size) {
    // Bit is set if block is uncompressed
    *compressed = !(hdr & SQUASHFS_COMPRESSED_BIT);
    *size = hdr & ~SQUASHFS_COMPRESSED_BIT;
    if (!*size)
        *size = SQUASHFS_COMPRESSED_BIT;
}

void sqsh_data_header(uint32_t hdr, bool *compressed, uint32_t *size) {
    *compressed = !(hdr & SQUASHFS_COMPRESSED_BIT_BLOCK);
    *size = hdr & ~SQUASHFS_COMPRESSED_BIT_BLOCK;
}

sqsh_err sqsh_block_read(struct sqsh_mount *ump, off_t pos, bool compressed,
    uint32_t size, size_t outsize, struct sqsh_block **block) {
	sqsh_err err;
    // allocate block on heap
    *block = malloc(sizeof(**block));
	if (*block == NULL)
		return SQFS_ERR;
    (*block)->data = malloc(size);
	if ((*block)->data == NULL)
		goto error;
    /*
		Currently we use an array of disk to allocate
		structures and verify metadata on read time.
		This is will change to vfs_() operations once driver
		successfully compiles.
    */
    memcpy((*block)->data, sqfs_image + pos, sizeof(struct sqsh_sb));

    // if block is compressed, first decompressed it and then initialize block
	if (compressed) {
		char *decomp = malloc(outsize);
		if (decomp == NULL)
			goto error;

		err = ump->decompressor->decompressor((*block)->data, size, decomp, &outsize);
		if (err != SQFS_OK) {
			free(decomp);
			goto error;
		}
		free((*block)->data);
		(*block)->data = decomp;
		(*block)->size = outsize;
	} else {
		(*block)->size = size;
	}

	return SQFS_OK;

error:
	sqsh_free_block(*block);
	*block = NULL;
	return err;
}

void sqsh_free_block(struct sqsh_block *block) {
	free(block->data);
	free(block);
}