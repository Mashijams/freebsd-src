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
#include <squashfs_dir.h>

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
	vap->va_type = vp->v_type;
	vap->va_mode = inode->base.mode;
	vap->va_nlink = inode->nlink;
	vap->va_gid = inode->base.guid;
	vap->va_uid = inode->base.uid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = inode->size;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_atime.tv_sec = inode->base.mtime;
	vap->va_ctime.tv_sec = inode->base.mtime;
	vap->va_mtime.tv_sec = inode->base.mtime;
	vap->va_birthtime.tv_sec = inode->base.mtime;
	vap->va_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
	    				inode->xtra.dev.major : NODEV;
	vap->va_filerev = 0;

	return (0);
}

static int
squashfs_read(struct vop_read_args *ap)
{
	TRACE("%s:",__func__);
	return (EOPNOTSUPP);
}

static int
squashfs_lookup(struct vop_cachedlookup_args *ap)
{
	TRACE("%s:",__func__);

	struct squashfs_mount *ump;
	struct sqsh_inode *inode;
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
	sqsh_err err;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;

	*vpp = NULLVP;
	inode = dvp->v_data;
	ump = inode->ump;

	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, curthread);
	if (error != 0)
		return (error);

	if (cnp->cn_flags & ISDOTDOT) {
		/* Do not allow .. on the root node */
		if (inode->xtra.dir.parent_inode == ump->sb.inodes + 1)
			return (ENOENT);

		/* Get inode number of parent inode */
		uint64_t i_ino;
		err = sqsh_export_inode(ump, inode->xtra.parent_inode, &i_ino);
		if (err != SQFS_OK)
			return (EINVAL);

		/* Allocate a new vnode on the matching entry */
		error = vn_vget_ino(dvp, i_ino, cnp->cn_lkflags, vpp);
		if (error != 0)
			return (error);
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
	} else {
		struct sqsh_dir_entry entry;
		bool found;

		found = false;

		/* Lookup for entry in directory, if found populate entry */
		err = sqsh_dir_lookup(ump, inode, cnp->cn_nameptr,
				cnp->cn_namelen, &entry, &found);
		if (err != SQFS_OK)
			return (EINVAL);

		if (found == false)
			return (ENOENT);

		error = VFS_VGET(ump->um_mountp, entry->inode_id, cnp->cn_lkflags, vpp);
		if (error != 0)
			return (error);
	}

	/* Store the result the the cache if MAKEENTRY is specified in flags */
	if ((cnp->cn_flags & MAKEENTRY) != 0 && cnp->cn_nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp);

	return (error);
}

static int
squashfs_readdir(struct vop_readdir_args *ap)
{
	TRACE("%s:",__func__);

	struct sqsh_mount *ump;
	struct dirent cde = { };
	struct sqsh_inode *inode;
	struct vnode *vp;
	struct uio *uio;
	struct sqsh_dir_entry entry;
	int *eofflag;
	uint64_t **cookies;
	int *ncookies;
	off_t off;
	u_int idx, ndirents;
	int error;
	sqsh_err err;

	vp = ap->a_vp;
	uio = ap->a_uio;
	eofflag = ap->a_eofflag;
	cookies = ap->a_cookies;
	ncookies = ap->a_ncookies;

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	inode = vp->v_data;
	ump = inode->ump;
	off = uio->uio_offset;
	ndirents = 0;

	if (uio->uio_offset == SQUASHFS_COOKIE_EOF)
		return (0);

	if (uio->uio_offset == SQUASHFS_COOKIE_DOT) {
		/* fake . entry */
		cde.d_fileno = inode->ino_id;
		cde.d_type = DT_DIR;
		cde.d_namlen = 1;
		cde.d_name[0] = '.';
		cde.d_name[1] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error)
			return (error);
		/* next is .. */
		uio->uio_offset = SQUASHFS_COOKIE_DOTDOT;
		ndirents++;
	}

	if (uio->uio_offset == SQUASHFS_COOKIE_DOTDOT) {
		/* fake .. entry */
		MPASS(inode->xtra.dir.parent_inode == ump->sb.inodes + 1);
		/* Get inode number of parent inode */
		uint64_t i_ino;
		err = sqsh_export_inode(ump, inode->xtra.parent_inode, &i_ino);
		if (err != SQFS_OK)
			return (EINVAL);
		cde.d_fileno = i_ino;
		cde.d_type = DT_DIR;
		cde.d_namlen = 2;
		cde.d_name[0] = '.';
		cde.d_name[1] = '.';
		cde.d_name[2] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error)
			return (error);
		/* next is first child */
		err = sqsh_dir_getnext(ump, inode->xtra.dir.d, &entry);
		if (err == SQFS_END_OF_DIRECTORY)
			goto done;
		if (err != SQFS_OK) {
			error = EINVAL;
			goto done;
		}
		uio->uio_offset = entry.inode_id;
		ndirents++;
	}

	for (;;) {
		cde.d_fileno = entry.inode_id;
		enum vtype type;
		type = sqsh_inode_type_from_id(ump, entry.inode_id);
		switch (type) {
		case VBLK:
			cde.d_type = DT_BLK;
			break;
		case VCHR:
			cde.d_type = DT_CHR;
			break;
		case VDIR:
			cde.d_type = DT_DIR;
			break;
		case VFIFO:
			cde.d_type = DT_FIFO;
			break;
		case VLNK:
			cde.d_type = DT_LNK;
			break;
		case VREG:
			cde.d_type = DT_REG;
			break;
		default:
			panic("%s: inode_type %d\n", __func__, type);
		}
		cde.d_namlen = entry.name_size;
		MPASS(entry.name_size < sizeof(cde.d_name));
		(void)memcpy(cde.d_name, entry.name, entry.name_size);
		cde.d_name[entry.name_size] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error != 0)
			goto done;
		ndirents++;
		/* next sibling */
		err = sqsh_dir_getnext(ump, inode->xtra.dir.d, &entry);
		if (err == SQFS_END_OF_DIRECTORY)
			goto done;
		if (err != SQFS_OK) {
			error = EINVAL;
			goto done;
		}
		uio->uio_offset = entry.inode_id;
	}

full:
	if (cde.d_reclen > uio->uio_resid)
		error = (ndirents == 0) ? EINVAL : 0;
done:
	TRACE("%s: %u entries written\n", __func__, ndirents);

	if (err == SQFS_END_OF_DIRECTORY)
		uio->uio_offset = SQUASHFS_COOKIE_EOF;

	if (eofflag != NULL) {
		TRACE("%s: Setting EOF flag\n", __func__);
		*eofflag = (error == 0 && err == SQFS_END_OF_DIRECTORY);
	}

	return (error);
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

	struct sqsh_inode *inode;
	struct vnode *vp;

	vp = ap->a_vp;
	inode = vp->v_data;

	printf("tag squashfs, squashfs_inode %p, links %lu\n",
	    inode, (unsigned long)inode->nlink);
	printf("\tmode 0%o, owner %d, group %d, size %zd\n",
	    inode->base.mode, inode->base.uid, inode->base.guid,
	    inode->size);

	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);

	printf("\n");

	return (0);
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