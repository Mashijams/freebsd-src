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
#include <squashfs_mount.h>
#include <squashfs_inode.h>
#include <squashfs_decompressor.h>


/* Support for zlib compressor */
#ifndef	GZIO

static const struct sqsh_decompressor sqsh_zlib_decompressor = {
	.decompressor	=	NULL,
	.id				=	ZLIB_COMPRESSION,
	.name			=	"zlib",
	.supported		=	0
};

#else	/* GZIO */

#include <contrib/zlib/zlib.h>

static sqsh_err
zlib_decompressor(void *input, size_t input_size, void *output, size_t *output_size)
{
	uLongf zout;
	int zerr;

	zout = *output_size;
	zerr = uncompress((Bytef*)output, &zout, input, input_size);
	if (zerr != Z_OK)
		return (SQFS_ERR);
	*output_size = zout;
	return (SQFS_OK);
}

const struct sqsh_decompressor sqsh_zlib_decompressor = {
	.decompressor	=	zlib_decompressor,
	.id				=	ZLIB_COMPRESSION,
	.name			=	"zlib",
	.supported		=	1
};

#endif	/* ZLIB */

/* lzma decompression support */
#ifndef	LZMA

static const struct sqsh_decompressor sqsh_lzma_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZMA_COMPRESSION,
	.name			=	"lzma",
	.supported		=	0
};

#endif /* LZMA */

/* lzo decompressor support */
#ifndef	LZO

static const struct sqsh_decompressor sqsh_lzo_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZO_COMPRESSION,
	.name			=	"lzo",
	.supported		=	0
};

#endif /* LZO */

/* lz4 decompressor supprt */
#ifndef	LZ4

static const struct sqsh_decompressor sqsh_lz4_decompressor = {
	.decompressor	=	NULL,
	.id				=	LZ4_COMPRESSION,
	.name			=	"lz4",
	.supported		=	0
};

#endif /* LZ4 */

/* zstd decompressor support */
#ifndef	ZSTDIO

static const struct sqsh_decompressor sqsh_zstd_decompressor = {
	.decompressor	=	NULL,
	.id				=	ZSTD_COMPRESSION,
	.name			=	"zstd",
	.supported		=	0
};

#else	/* ZSTDIO */

#include <contrib/zstd/lib/zstd.h>

static sqsh_err
zstd_decompressor(void *input, size_t input_size, void *output, size_t *output_size)
{
	const size_t zstdout;

	zstdout = ZSTD_decompress(output, *output_size, input, input_size);
	if (ZSTD_isError(zstdout))
		return (SQFS_ERR);
	*output_size = zstdout;
	return (SQFS_OK);
}

const struct sqsh_decompressor sqsh_zstd_decompressor = {
	.decompressor	=	zstd_decompressor,
	.id				=	ZSTD_COMPRESSION,
	.name			=	"zstd",
	.supported		=	1
};

#endif	/* ZSTDIO */

/* Unknown compression type */
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
	&sqsh_zstd_decompressor,
	&sqsh_unknown_decompressor
};

const struct sqsh_decompressor *
sqsh_lookup_decompressor(int id)
{
	int i;

	for (i = 0; decompressor[i]->id; i++)
		if (id == decompressor[i]->id)
			break;

	return decompressor[i];
}