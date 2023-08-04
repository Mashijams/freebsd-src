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

#ifndef	SQUASHFS_DECOMPRESSOR_H
#define	SQUASHFS_DECOMPRESSOR_H

#include "opt_gzio.h"
#include "opt_zstdio.h"

struct sqsh_decompressor {
	sqsh_err (*decompressor)(void* input, size_t input_size,
		void* output, size_t* output_size);

	int		id;
	char*	name;
	int		supported;
};

#ifdef	GZIO
extern const struct sqsh_decompressor sqsh_zlib_decompressor;
#endif	/* GZIO */

#ifdef	LZMA
extern const struct sqsh_decompressor sqsh_lzma_decompressor;
#endif	/* LZMA */

#ifdef	LZO
extern const struct sqsh_decompressor sqsh_lzo_decompressor;
#endif	/* LZO */

#ifdef	LZ4
extern const struct sqsh_decompressor sqsh_lz4_decompressor;
#endif	/* LZ4 */

#ifdef	ZSTDIO
extern const struct sqsh_decompressor sqsh_zstd_decompressor;
#endif	/* ZSTDIO */

const struct sqsh_decompressor	*sqsh_lookup_decompressor(int id);

#endif	/* SQUASHFS_DECOMPRESSOR_H */