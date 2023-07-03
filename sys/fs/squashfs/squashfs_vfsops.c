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

#include<squashfs.h>
#include<squashfs_io.h>
#include<squashfs_mount.h>
#include<squashfs_inode.h>
#include<squashfs_decompressor.h>

static	MALLOC_DEFINE(M_SQUASHFSMNT, "SQUASHFS mount", "SQUASHFS mount structure");
static	MALLOC_DEFINE(M_SQUASHFS_NODE, "SQUASHFS inode", "SQUASHFS vnode private data");

static	vfs_mount_t		squashfs_mount;
static	vfs_unmount_t	squashfs_unmount;
static	vfs_root_t		squashfs_root;
static	vfs_statfs_t	squashfs_statfs;
static	vfs_vget_t		squashfs_vget;
static	vfs_fhtovp_t	squashfs_fhtovp;

static void
squashfs_swapendian_sb(struct sqsh_sb *sb)
{
	sb->s_magic					=	le32toh(sb->s_magic);
	sb->inodes					=	le32toh(sb->inodes);
	sb->mkfs_time				=	le32toh(sb->mkfs_time);
	sb->block_size				=	le32toh(sb->block_size);
	sb->fragments				=	le32toh(sb->fragments);
	sb->compression				=	le16toh(sb->compression);
	sb->block_log				=	le16toh(sb->block_log);
	sb->flags					=	le16toh(sb->flags);
	sb->no_ids					=	le16toh(sb->no_ids);
	sb->s_major					=	le16toh(sb->s_major);
	sb->s_minor					=	le16toh(sb->s_minor);
	sb->root_inode				=	le64toh(sb->root_inode);
	sb->bytes_used				=	le64toh(sb->bytes_used);
	sb->id_table_start			=	le64toh(sb->id_table_start);
	sb->xattr_id_table_start	=	le64toh(sb->xattr_id_table_start);
	sb->inode_table_start		=	le64toh(sb->inode_table_start);
	sb->directory_table_start	=	le64toh(sb->directory_table_start);
	sb->fragment_table_start	=	le64toh(sb->fragment_table_start);
	sb->lookup_table_start		=	le64toh(sb->lookup_table_start);
}

static sqsh_err
is_valid_superblock(struct sqsh_sb* sb)
{
	// Check magic number
	if (sb->s_magic != SQUASHFS_MAGIC && sb->s_magic != SQUASHFS_MAGIC_SWAP) {
		ERROR("Bad superblock magic number");
		return SQFS_BADFORMAT;
	}

	// Check for version of mounted fs
	if (sb->s_major != SQUASHFS_MAJOR || sb->s_minor > SQUASHFS_MINOR) {
		ERROR("Unsupported version of squashfs is mounted");
		return SQFS_BADVERSION;
	}

	// Check if filesystem size is not negative for sanity
	if (sb->bytes_used < 0) {
		ERROR("Filesystem size is negative!");
		return SQFS_ERR;
	}

	// Check block size for sanity
	if (sb->block_size > SQUASHFS_FILE_MAX_SIZE) {
		ERROR("Invalid block size");
		return SQFS_ERR;
	}

	// Check block log for sanity
	if (sb->block_log > SQUASHFS_FILE_MAX_LOG) {
		ERROR("Invalid block log");
		return SQFS_ERR;
	}

	// Check that block_size and block_log match
	if (sb->block_size != (1 << sb->block_log)) {
		ERROR("Block size and log mismatch");
		return SQFS_ERR;
	}

	// Check the root inode for sanity
	if (SQUASHFS_INODE_OFFSET(sb->root_inode) > SQUASHFS_METADATA_SIZE) {
		ERROR("Invalid root inode size");
		return SQFS_ERR;
	}

	// A valid superblock is detected
	TRACE("A valid superblock is detected");
	return SQFS_OK;
}

static sqsh_err
squashfs_init(struct sqsh_mount* ump)
{
	// squashfs superblock is at offset zero
	if (sqsh_io_read_buf(ump, &ump->sb, 0, sizeof(struct sqsh_sb)) !=
			sizeof(struct sqsh_sb)) {
			ERROR("Failed to read superblock, I/O error");
			return SQFS_ERR;
	}
	squashfs_swapendian_sb(&ump->sb);

	// check superblock to see if everything is fine
	sqsh_err error = is_valid_superblock(&ump->sb);
	if (error != SQFS_OK)
		return error;

	// Init decompressor for squashfs and check if it is unknown or supported?
	ump->decompressor = sqsh_lookup_decompressor(ump->sb.compression);
	if (!ump->decompressor->supported) {
		ERROR("Filesystem uses \"%s\" compression. This is not supported",
		       ump->decompressor->name);
		return SQFS_BADCOMP;
	}

	error = sqsh_init_table(&ump->id_table, ump, ump->sb.id_table_start,
		sizeof(uint32_t), ump->sb.no_ids);
	if (error != SQFS_OK)
		goto id_table_fail;

	error = sqsh_init_table(&ump->frag_table, ump, ump->sb.fragment_table_start,
		sizeof(struct sqsh_fragment_entry), ump->sb.fragments);
	if (error != SQFS_OK)
		goto frag_table_fail;

	if (sqsh_export_ok(ump)) {
		error = sqsh_init_table(&ump->export_table, ump, ump->sb.lookup_table_start,
			sizeof(uint64_t), ump->sb.inodes);
		if (error != SQFS_OK)
			goto export_table_fail;
	}

	TRACE("Table init() passed!");

	// TODO : add checks for caches after implementing it

	// Everything fine
	return SQFS_OK;

id_table_fail:
	sqsh_free_table(&ump->id_table);
	return error;

frag_table_fail:
	sqsh_free_table(&ump->id_table);
	sqsh_free_table(&ump->frag_table);
	return error;

export_table_fail:
	sqsh_free_table(&ump->id_table);
	sqsh_free_table(&ump->frag_table);
	sqsh_free_table(&ump->export_table);
	return error;

}

// VFS operations
static int
squashfs_mount(struct mount* mp)
{
	struct nameidata nd;
	struct sqsh_mount *ump = NULL;
	struct vnode *vp;
	struct thread *td = curthread;
	char *from, *as;
	int error, len, aslen, flags;


	TRACE("squashfs_mount(mp = %p)\n", mp);

	if (mp->mnt_flag & MNT_ROOTFS) {
		vfs_mount_error(mp, "Cannot mount root filesystem");
		return (EOPNOTSUPP);
	}

	if (mp->mnt_flag & MNT_UPDATE) {
		vfs_mount_error(mp, "squashfs does not support mount update");
		return (EOPNOTSUPP);
	}

	// Get argument
	error = vfs_getopt(mp->mnt_optnew, "from", (void **)&from, &len);
	if (error != 0 || from[len - 1] != '\0')
		return (EINVAL);
	error = vfs_getopt(mp->mnt_optnew, "as", (void **)&as, &aslen);
	if (error || as[aslen - 1] != '\0')
		as = from;

	// find and initialise squashfs disk file vnode vp
	NDINIT(&nd, LOOKUP, ISOPEN | FOLLOW | LOCKLEAF, UIO_SYSSPACE, from, td);
	error = namei(&nd);
	if (error != 0)
		return (error);
	NDFREE_PNBUF(&nd);
	vp = nd.ni_vp;
	// vp is now held and locked

	// open the file
	flags = FREAD;
	error = vn_open_vnode(vp, flags, td->td_ucred, td, NULL);
	if (error != 0) {
		ERROR("Failed to open squashfs disk file");
		vput(vp);
		return error;
	}

	// check if vnode is of file type (squashfs disk is always of regular file type)
	if (vp->v_type != VREG) {
		ERROR("Squashfs disk is not regular file");
		error = EOPNOTSUPP;
		VOP_UNLOCK(vp);
		return error;
	}

	// check if file is not private
	error = priv_check(td, PRIV_VFS_MOUNT_PERM);
	if (error != 0) {
		ERROR("Squashfs disk is private file");
		error = EOPNOTSUPP;
		VOP_UNLOCK(vp);
		return error;
	}

	// Create squashfs mount
	ump = malloc(sizeof(struct sqsh_mount), M_SQUASHFSMNT,
	    M_WAITOK | M_ZERO);
	ump->um_mountp = mp;
	ump->um_vp = vp;

	sqsh_err err = squashfs_init(ump);

	switch (err) {
		case SQFS_OK:
			break;
		case SQFS_BADFORMAT:
			ERROR("Wrong squashfs image");
			break;
		case SQFS_BADVERSION:
			ERROR("Squashfs 4.0 to 4.%d is supported", SQUASHFS_MINOR);
			break;
		case SQFS_BADCOMP:
			break;
		default:
			ERROR("Some unknown error happend while mounting squashfs image");
	}

	if (err != SQFS_OK)
		goto failed_mount;

	mp->mnt_data = ump;
	mp->mnt_stat.f_iosize = SQUASHFS_IO_SIZE;

	// Unconditionally mount squashfs as read only
	MNT_ILOCK(mp);
	mp->mnt_flag |= (MNT_LOCAL | MNT_RDONLY);
	MNT_IUNLOCK(mp);

	vfs_getnewfsid(mp);
	vfs_mountedfrom(mp, as);
	TRACE("Squashfs mount successful");
	return (0);

failed_mount:
	TRACE("Squashfs mount failed");
	(void)vn_close(vp, flags, td->td_ucred, td);
	free(ump, M_SQUASHFSMNT);
	return EINVAL;
}

static int
squashfs_unmount(struct mount *mp, int mntflags)
{
	TRACE("%s:",__func__);
	struct thread *td = curthread;
	struct sqsh_mount *ump;
	struct vnode *vp;
	int flags = FREAD;

	// Handle forced unmounts
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	ump = MP_TO_SQSH_MOUNT(mp);
	vp = ump->um_vp;

	// close disk file vnode
	vn_close(vp, flags, td->td_ucred, td);

	// destroy fs internals
	sqsh_free_table(&ump->id_table);
	sqsh_free_table(&ump->frag_table);
	if (sqsh_export_ok(ump))
		sqsh_free_table(&ump->export_table);

	free(ump, M_SQUASHFSMNT);
	TRACE("%s: completed",__func__);

	return (0);
}

static int
squashfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	TRACE("%s:",__func__);
	struct vnode *nvp;
	struct sqsh_mount *ump;

	ump = MP_TO_SQSH_MOUNT(mp);

	int error = VFS_VGET(mp, sqsh_root_inode(ump), LK_EXCLUSIVE, &nvp);
	if (error != 0)
		return error;

	nvp->v_vflag |= VV_ROOT;
	*vpp = nvp;
	TRACE("%s: completed",__func__);
	return (0);
}

static int
squashfs_statfs(struct mount *mp, struct statfs *sbp)
{
	TRACE("%s:",__func__);
	struct sqsh_mount *ump;
	ump = MP_TO_SQSH_MOUNT(mp);

	sbp->f_bsize	=	ump->sb.block_size;
	sbp->f_iosize	=	SQUASHFS_IO_SIZE;
	sbp->f_blocks	=	ump->sb.bytes_used / ump->sb.block_size;
	sbp->f_bfree	=	0;
	sbp->f_bavail	=	0;
	sbp->f_files	=	ump->sb.inodes;
	sbp->f_ffree	=	0;

	return (0);
}

static int
squashfs_vget(struct mount *mp, ino_t ino, int lkflags, struct vnode **vpp)
{
	TRACE("%s:",__func__);
	struct sqsh_mount *ump;
	struct sqsh_inode *inode;
	struct thread *td;
	struct vnode *vp;

	td = curthread;
	int error = vfs_hash_get(mp, ino, lkflags, td, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	ump = MP_TO_SQSH_MOUNT(mp);
	inode = malloc(sizeof(struct sqsh_inode), M_SQUASHFS_NODE, M_WAITOK | M_ZERO);

	// populate inode data as per inode number
	sqsh_err err = sqsh_get_inode(ump, inode, ino);
	if (err != SQFS_OK) {
		*vpp = NULL;
		free(inode, M_SQUASHFS_NODE);
		return EINVAL;
	}

	error = getnewvnode("squashfs", mp, &squashfs_vnodeops, &vp);
	if (error != 0) {
		*vpp = NULL;
		free(inode, M_SQUASHFS_NODE);
		return error;
	}

	vp->v_data = inode;
	vp->v_type = inode->base.inode_type;
	inode->vnode = vp;

	lockmgr(vp->v_vnlock, lkflags, NULL);
	error = insmntque(vp, mp);
	if (error != 0) {
		*vpp = NULL;
		free(inode, M_SQUASHFS_NODE);
		return error;
	}
	error = vfs_hash_insert(vp, ino, lkflags, td, vpp, NULL, NULL);
	if (error != 0 || *vpp != NULL)
		return (error);

	//vn_set_state(vp, VSTATE_CONSTRUCTED);
	TRACE("%s: completed",__func__);
	*vpp = vp;
	return (0);
}

static int
squashfs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	TRACE("%s:",__func__);
	struct sqsh_fid *tfp;
	struct vnode *vp;

	tfp = (struct sqsh_fid*)fhp;

	int error = VFS_VGET(mp, tfp->ino, LK_EXCLUSIVE, &vp);
	if (error != 0) {
		*vpp = NULL;
		return error;
	}
	// TODO : add checks for inode

	*vpp = vp;
	TRACE("%s: completed",__func__);
	return (0);
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