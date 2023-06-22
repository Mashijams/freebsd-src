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
#include<squashfs_mount.h>
#include<squashfs_inode.h>
#include<squashfs_decompressor.h>


// Support for zlib compressor
#ifndef SQUASHFS_ZLIB

static const struct sqsh_decompressor sqsh_zlib_decompressor = {
	.decompressor	=	NULL,
	.id				=	ZLIB_COMPRESSION,
	.name			=	"zlib",
	.supported		=	0
};

#else

#include <zlib.h>

static sqsh_err zlib_decompressor(void *input, size_t input_size,
		void *output, size_t *output_size) {
	uLongf zout = *output_size;
	int zerr = uncompress((Bytef*)output, &zout, input, input_size);
	if (zerr != Z_OK)
		return SQFS_ERR;
	*output_size = zout;
	return SQFS_OK;
}

const struct sqsh_decompressor sqsh_zlib_decompressor = {
	.decompressor	=	zlib_decompressor,
	.id				=	ZLIB_COMPRESSION,
	.name			=	"zlib",
	.supported		=	1
};

#endif // SQUASHFS_ZLIB

// lzma decompression support
#ifndef SQUASHFS_LZMA

static const struct sqsh_decompressor sqsh_lzma_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZMA_COMPRESSION,
	.name			=	"lzma",
	.supported		=	0
};

#else

#include <lzma.h>

static sqsh_err lzma_decompressor(void *input, size_t input_size,
		void *output, size_t *output_size) {
	uint64_t memlimit = UINT64_MAX;
	size_t inpos = 0, outpos = 0;
	lzma_ret err = lzma_stream_buffer_decode(&memlimit, 0, NULL, input, &inpos, input_size,
		output, &outpos, *output_size);
	if (err != LZMA_OK)
		return SQFS_ERR;
	*output_size = outpos;
	return SQFS_OK;
}

const struct sqsh_decompressor sqsh_lzma_decompressor = {
	.decompressor	=	lzma_decompressor,
	.id				=	LZMA_COMPRESSION,
	.name			=	"lzma",
	.supported		=	1
};

#endif // SQUASHFS_LZMA

// lzo decompressor support
#ifndef SQUASHFS_LZO

static const struct sqsh_decompressor sqsh_lzo_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZO_COMPRESSION,
	.name			=	"lzo",
	.supported		=	0
};

#else

#include <lzo/lzo1x.h>

static sqsh_err lzo_decompressor(void *input, size_t input_size,
		void *output, size_t *output_size) {
	lzo_uint lzout = *output_size;
	int err = lzo1x_decompress_safe(input, input_size, output, &lzout, NULL);
	if (err != LZO_E_OK)
		return SQFS_ERR;
	*output_size = lzout;
	return SQFS_OK;
}

const struct sqsh_decompressor sqsh_lzo_decompressor = {
	.decompressor	=	lzo_decompressor,
	.id				=	LZO_COMPRESSION,
	.name			=	"lzo",
	.supported		=	1
};

#endif // SQUASHFS_LZO


// lz4 decompressor supprt
#ifndef SQUASHFS_LZ4

static const struct sqsh_decompressor sqsh_lz4_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZ4_COMPRESSION,
	.name			=	"lz4",
	.supported		=	0
};

#else

#include <lz4.h>

static sqsh_err lz4_decompressor(void *input, size_t input_size,
		void *output, size_t *output_size) {
	int lz4out = LZ4_decompress_safe (input, output, input_size, *output_size);
	if (lz4out < 0)
		return SQFS_ERR;
	*output_size = lz4out;
	return SQFS_OK;
}

const struct sqsh_decompressor sqsh_lz4_decompressor = {
	.decompressor	=	lz4_decompressor,
	.id				=	LZ4_COMPRESSION,
	.name			=	"lz4",
	.supported		=	1
};

#endif // SQUASHFS_LZ4

// Unknown compression type
static const struct sqsh_decompressor sqsh_unknown_decompressor = {
	.decompressor	=	NULL,
	.id				=	0,
	.name			=	"unknown",
	.supported		=	0
};


static const struct sqsh_decompressor *decompressor[] = {
	&sqsh_zlib_decompressor,
	&sqsh_lzma_decompressor,
	&sqsh_lzo_decompressor,
	&sqsh_lz4_decompressor,
	&sqsh_unknown_decompressor
};

const struct sqsh_decompressor *sqsh_lookup_decompressor(int id)
{
	int i;

	for (i = 0; decompressor[i]->id; i++)
		if (id == decompressor[i]->id)
			break;

	return decompressor[i];
}