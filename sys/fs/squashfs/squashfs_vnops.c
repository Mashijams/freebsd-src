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

#include<fs/squashfs/squashfs.h>
#include<fs/squashfs/squashfs_bin.h>
#include<fs/squashfs/squashfs_mount.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

static int
squashfs_lookup(struct vop_cachedlookup_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_create(struct vop_create_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_whiteout(struct vop_whiteout_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_mknod(struct vop_mknod_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_open(struct vop_open_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_close(struct vop_close_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_access(struct vop_access_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_getattr(struct vop_getattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_setattr(struct vop_setattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_read(struct vop_read_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_write(struct vop_write_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_ioctl(struct vop_ioctl_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_poll(struct vop_poll_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_fsync(struct vop_fsync_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_remove(struct vop_remove_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_link(struct vop_link_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_rename(struct vop_rename_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_mkdir(struct vop_mkdir_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_rmdir(struct vop_rmdir_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_symlink(struct vop_symlink_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_readdir(struct vop_readdir_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_readlink(struct vop_readlink_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_getwritemount(struct vop_getwritemount_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_inactive(struct vop_inactive_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_reclaim(struct vop_reclaim_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_print(struct vop_print_args *ap)
{
	return (EOPNOTSUPP);
}

static void
squashfs_revlock(struct vnode *vp, int flags)
{
	return (EOPNOTSUPP);
}

static int
squashfs_lock(struct vop_lock1_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_unlock(struct vop_unlock_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_pathconf(struct vop_pathconf_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_advlock(struct vop_advlock_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_strategy(struct vop_strategy_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_getacl(struct vop_getacl_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_setacl(struct vop_setacl_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_aclcheck(struct vop_aclcheck_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_openextattr(struct vop_openextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_closeextattr(struct vop_closeextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_getextattr(struct vop_getextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_setextattr(struct vop_setextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_listextattr(struct vop_listextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_deleteextattr(struct vop_deleteextattr_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_setlabel(struct vop_setlabel_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_vptofh(struct vop_vptofh_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_add_writecount(struct vop_add_writecount_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_vput_pair(struct vop_vput_pair_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_set_text(struct vop_set_text_args *ap)
{
	return (EOPNOTSUPP);
}

static int
squashfs_unset_text(struct vop_unset_text_args *ap)
{
	return (EOPNOTSUPP);
}


struct vop_vector squashfs_vnodeops = {
	.vop_default		=	&default_vnodeops,

	.vop_access			=	squashfs_access,
	.vop_aclcheck		=	squashfs_aclcheck,
	.vop_advlock		=	squashfs_advlock,
	.vop_bmap			=	squashfs_bmap,
	.vop_cachedlookup	=	squashfs_lookup,
	.vop_close			=	squashfs_close,
	.vop_closeextattr	=	squashfs_closeextattr,
	.vop_create			=	squashfs_create,
	.vop_deleteextattr	=	squashfs_deleteextattr,
	.vop_fsync			=	squashfs_fsync,
	.vop_getacl			=	squashfs_getacl,
	.vop_getattr		=	squashfs_getattr,
	.vop_getextattr		=	squashfs_getextattr,
	.vop_getwritemount	=	squashfs_getwritemount,
	.vop_inactive		=	squashfs_inactive,
	.vop_ioctl			=	squashfs_ioctl,
	.vop_link			=	squashfs_link,
	.vop_listextattr	=	squashfs_listextattr,
	.vop_lock1			=	squashfs_lock,
	.vop_lookup			=	vfs_cache_lookup,
	.vop_mkdir			=	squashfs_mkdir,
	.vop_mknod			=	squashfs_mknod,
	.vop_open			=	squashfs_open,
	.vop_openextattr	=	squashfs_openextattr,
	.vop_pathconf		=	squashfs_pathconf,
	.vop_poll			=	squashfs_poll,
	.vop_print			=	squashfs_print,
	.vop_read			=	squashfs_read,
	.vop_readdir		=	squashfs_readdir,
	.vop_readlink		=	squashfs_readlink,
	.vop_reclaim		=	squashfs_reclaim,
	.vop_remove			=	squashfs_remove,
	.vop_rename			=	squashfs_rename,
	.vop_rmdir			=	squashfs_rmdir,
	.vop_setacl			=	squashfs_setacl,
	.vop_setattr		=	squashfs_setattr,
	.vop_setextattr		=	squashfs_setextattr,
	.vop_setlabel		=	squashfs_setlabel,
	.vop_strategy		=	squashfs_strategy,
	.vop_symlink		=	squashfs_symlink,
	.vop_unlock			=	squashfs_unlock,
	.vop_whiteout		=	squashfs_whiteout,
	.vop_write			=	squashfs_write,
	.vop_vptofh			=	squashfs_vptofh,
	.vop_add_writecount	=	squashfs_add_writecount,
	.vop_vput_pair		=	squashfs_vput_pair,
	.vop_set_text		=	squashfs_set_text,
	.vop_unset_text		=	squashfs_unset_text,
};
VFS_VOP_VECTOR_REGISTER(squashfs_vnodeops);