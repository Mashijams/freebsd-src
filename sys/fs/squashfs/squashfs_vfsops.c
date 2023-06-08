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

#include <vm/vm_param.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include<fs/squashfs/squashfs.h>
#include<fs/squashfs/squashfs_bin.h>
#include<fs/squashfs/squashfs_mount.h>

static	MALLOC_DEFINE(M_SQUASHFSMNT, "SQUASHFS mount", "SQUASHFS mount structure");

static	vfs_mount_t		squashfs_mount;
static	vfs_unmount_t	squashfs_unmount;
static	vfs_root_t		squashfs_root;
static	vfs_statfs_t	squashfs_statfs;
static	vfs_vget_t		squashfs_vget;
static	vfs_fhtovp_t	squashfs_fhtovp;

static sqsh_err
squashfs_init(struct sqsh_mount* ump)
{
	// implement here
}

// VFS operations
static int
squashfs_mount(struct mount* mp)
{
	struct sqsh_mount *ump = NULL;
	char *target;
	int error, len;


	DEBUG("squashfs_mount(mp = %p)\n", mp);

	if (mp->mnt_flag & MNT_ROOTFS) {
		vfs_mount_error(mp, "Cannot mount root filesystem");
		return (EOPNOTSUPP);
	}

	if (mp->mnt_flag & MNT_UPDATE) {
		vfs_mount_error(mp, "squashfs does not support mount update");
		return (EOPNOTSUPP);
	}

	// Get argument
	error = vfs_getopt(mp->mnt_optnew, "from", (void **)&target, &len);
	if (error != 0)
		error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target, &len);
	if (error || target[len - 1] != '\0') {
		vfs_mount_error(mp, "Invalid target");
		return (EINVAL);
	}

	// Create squashfs mount
	ump = malloc(sizeof(struct sqsh_mount), M_SQUASHFSMNT,
	    M_WAITOK | M_ZERO);
	ump->um_mountp = mp;

	sqsh_err err = squashfs_init(ump);

	switch (err) {
		case SQFS_OK:
			break;
		case SQFS_BADFORMAT:
			ERROR("Wrong squashfs image");
			break;
		case SQFS_BADVERSION:
			break;
		case SQFS_BADCOMP:
			break;
		default:
			ERROR("Some unknown error happend while mounting squashfs image");
	}

	if (err != SQFS_OK)
		return (EINVAL);

	mp->mnt_data = ump;
	return (0);
}

static int
squashfs_unmount(struct mount *mp, int mntflags)
{
	return (EOPNOTSUPP);
}

static int
squashfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

static int
squashfs_statfs(struct mount *mp, struct statfs *sbp)
{
	return (EOPNOTSUPP);
}

static int
squashfs_vget(struct mount *mp, ino_t ino, int lkflags, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

static int
squashfs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	return (EOPNOTSUPP);
}

static struct vfsops squashfs_vfsops = {
	.vfs_fhtovp		=	squashfs_fhtovp,
	.vfs_mount		=	squashfs_mount,
	.vfs_root		=	squashfs_root,
	.vfs_statfs		=	squashfs_statfs,
	.vfs_unmount	=	squashfs_unmount,
	.vfs_vget		=	squashfs_vget,
};

VFS_SET(squashfs_vfsops, squashfs, VFCF_READONLY);