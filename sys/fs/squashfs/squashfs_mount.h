/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Raghav Sharma <raghav@freebsd.org>
 * All rights reserved.
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

#ifndef	SQUASHFS_MOUNT_H
#define	SQUASHFS_MOUNT_H

#ifdef	_KERNEL

/* This structure describes squashfs mount structure data */
struct sqsh_mount {
	struct mount					*um_mountp;
	struct vnode					*um_vp;
	struct sqsh_sb					sb;
	struct sqsh_table				id_table;
	struct sqsh_table				frag_table;
	struct sqsh_table				export_table;
	struct sqsh_table				xattr_table;
	struct sqsh_xattr_id_table		xattr_info;
	const struct sqsh_decompressor	*decompressor;
};

static inline struct sqsh_mount *
MP_TO_SQSH_MOUNT(struct mount *mp)
{
	MPASS(mp != NULL && mp->mnt_data != NULL);
	return (mp->mnt_data);
}

#endif	/* _KERNEL */

#endif	/* SQUASHFS_MOUNT_H */