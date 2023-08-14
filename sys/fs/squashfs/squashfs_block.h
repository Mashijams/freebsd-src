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

#ifndef	SQUASHFS_BLOCK_H
#define	SQUASHFS_BLOCK_H

struct sqsh_block {
	size_t	size;
	void*	data;
};

struct sqsh_block_run {
	off_t	block;
	size_t	offset;
};

struct sqsh_dir {
	struct sqsh_block_run	cur;
	off_t					offset;
	off_t					total;
	struct sqsh_dir_header	header;
};

/* Helper functions to check if metadata/data block is compressed and its size */
void		sqsh_metadata_header(uint16_t hdr, bool *compressed, uint16_t *size);
void		sqsh_data_header(uint32_t hdr, bool *compressed, uint32_t *size);

sqsh_err	sqsh_block_read(struct sqsh_mount *ump, off_t pos, bool compressed,
				uint32_t size, size_t outsize, struct sqsh_block **block);
void		sqsh_free_block(struct sqsh_block *block);

sqsh_err	sqsh_metadata_read(struct sqsh_mount *ump, off_t pos, size_t *data_size,
				struct sqsh_block **block);
sqsh_err	sqsh_data_read(struct sqsh_mount *ump, off_t pos, uint32_t hdr,
				struct sqsh_block **block);

sqsh_err	sqsh_metadata_get(struct sqsh_mount *ump, struct sqsh_block_run *cur,
				void *buf, size_t size);

/* Number of groups of size "group" required to hold size "total" */
size_t		sqsh_ceil(uint64_t total, size_t group);

#endif