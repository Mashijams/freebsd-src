/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Raghav Sharma <raghav@freebsd.org>
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
 * $FreeBSD$
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
#include<squashfs_io.h>

/*
	Reads data according to the provided uio.
	This function reads directly from disk file
	and all decompression reads are handled by seperate
	functions in squashfs_block.h file.
*/
sqsh_err sqsh_io_read(struct sqsh_mount *ump, struct uio *uiop) {
	void *rl	=	NULL;
	off_t off	=	uiop->uio_offset;
	size_t len	=	uiop->uio_resid;

	rl = vn_rangelock_rlock(ump->um_vp, off, off + len);
	int error = vn_lock(ump->um_vp, LK_SHARED);
	if (error != 0)
		return SQFS_ERR;

	error = VOP_READ(ump->um_vp, uiop, IO_DIRECT|IO_NODELOCKED,
		uiop->uio_td->td_ucred);
	VOP_UNLOCK(ump->vp);
	vn_rangelock_unlock(ump->um_vp, rl);

	if (error != 0)
		return SQFS_ERR;

	return SQFS_OK;
}

/*
	Reads data into the provided buffer.
	This function reads directly from disk file
	and all decompression reads are handled by seperate
	functions in squashfs_block.h file.
	On succes it return number of bytes read else negative
	value on failure.
*/
ssize_t sqsh_io_read_buf(struct sqsh_mount *ump, void *buf, off_t off, size_t len) {
	struct uio auio;
	struct iovec aiov;

	// return success and reading zero bytes of data
	if (len == 0)
		return 0;

	// initialize iovec
	aiov.iov_base	=	buf;
	aiov.iov_len	=	len;

	// initialize uio
	auio.uio_iov	=	&aiov;
	auio.uio_iovcnt	=	1;
	auio.uio_offset	=	off;
	auio.uio_segflg	=	UIO_SYSSPACE;
	auio.uio_rw		=	UIO_READ;
	auio.uio_resid	=	len;
	auio.uio_td		=	curthread;

	sqsh_err error	=	sqsh_io_read(tmp, &auio);

	// return negative value on reading failure
	if (error != SQFS_OK)
		return -1;

	ssize_t res = len - auio.uio_resid;

	return res;
}