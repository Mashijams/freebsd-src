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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/kdb.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

#include <squashfs.h>
#include <squashfs_mount.h>
#include <squashfs_inode.h>

static int
squashfs_lookup(struct vop_cachedlookup_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_open(struct vop_open_args *ap)
{
	TRACE("%s:",__func__);

	struct sqsh_inode *inode;
	struct vnode *vp;

	vp = ap->a_vp;
	MPASS(VOP_ISLOCKED(vp));
	inode = vp->v_data;

	if (vp->v_type != VREG && vp->v_type != VDIR)
		return (EOPNOTSUPP);

	vnode_create_vobject(vp, inode->size, ap->a_td);
	return (0);
}

static int
squashfs_close(struct vop_close_args *ap)
{
	TRACE("%s:",__func__);
	return (0);
}

static int
squashfs_access(struct vop_access_args *ap)
{
	TRACE("%s:",__func__);

	struct sqsh_inode *inode;
	struct vnode *vp;
	accmode_t accmode;
	struct ucred *cred;
	int error;

	vp = ap->a_vp;
	accmode = ap->a_accmode;
	cred = ap->a_cred;

	MPASS(VOP_ISLOCKED(vp));
	inode = vp->v_data;

	switch (vp->v_type) {
	case VDIR:
	case VLNK:
	case VREG:
		if ((accmode & VWRITE) != 0)
			return (EROFS);
		break;
	case VBLK:
	case VCHR:
	case VFIFO:
	case VSOCK:
		break;
	default:
		return (EINVAL);
	}

	if ((accmode & VWRITE) != 0)
		return (EPERM);

	error = vaccess(vp->v_type, inode->base.mode, inode->base.uid,
	    inode->base.guid, accmode, cred);

	return (error);
}

static int
squashfs_getattr(struct vop_getattr_args *ap)
{
	TRACE("%s:",__func__);

	struct sqsh_inode *inode;
	struct vnode *vp;
	struct vattr *vap;

	vp		=	ap->a_vp;
	vap		=	ap->a_vap;
	inode	=	vp->v_data;

	/* fill up vattr for squashfs inode */
	vap->va_type		=	vp->v_type;
	vap->va_mode		=	inode->base.mode;
	vap->va_nlink		=	inode->nlink;
	vap->va_gid			=	inode->base.guid;
	vap->va_uid			=	inode->base.uid;
	vap->va_fsid		=	vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size		=	inode->size;
	vap->va_blocksize	=	vp->v_mount->mnt_stat.f_iosize;
	vap->va_atime		=	inode->base.mtime;
	vap->va_ctime		=	inode->base.mtime;
	vap->va_mtime		=	inode->base.mtime;
	vap->va_birthtime	=	inode->base.mtime;
	vap->va_rdev		=	(vp->v_type == VBLK || vp->v_type == VCHR) ?
	    						tnp->rdev : NODEV;
	vap->va_filerev		=	0;

	return (0);
}

static int
squashfs_read(struct vop_read_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_readdir(struct vop_readdir_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_readlink(struct vop_readlink_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_reclaim(struct vop_reclaim_args *ap)
{
	TRACE("%s:",__func__);
	struct sqsh_inode *inode;
	struct vnode *vp;

	vp = ap->a_vp;
	inode = vp->v_data;

	vfs_hash_remove(vp);

	inode->vnode = NULLVP;
	vp->v_data = NULL;

	TRACE("%s: completed",__func__);
	return (0);
}

static int
squashfs_print(struct vop_print_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_strategy(struct vop_strategy_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_vptofh(struct vop_vptofh_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}


struct vop_vector squashfs_vnodeops = {
	.vop_default		=	&default_vnodeops,

	.vop_access			=	squashfs_access,
	.vop_cachedlookup	=	squashfs_lookup,
	.vop_close			=	squashfs_close,
	.vop_getattr		=	squashfs_getattr,
	.vop_lookup			=	vfs_cache_lookup,
	.vop_open			=	squashfs_open,
	.vop_print			=	squashfs_print,
	.vop_read			=	squashfs_read,
	.vop_readdir		=	squashfs_readdir,
	.vop_readlink		=	squashfs_readlink,
	.vop_reclaim		=	squashfs_reclaim,
	.vop_strategy		=	squashfs_strategy,
	.vop_vptofh			=	squashfs_vptofh,
};

VFS_VOP_VECTOR_REGISTER(squashfs_vnodeops);