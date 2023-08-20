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
#include <squashfs_io.h>
#include <squashfs_mount.h>
#include <squashfs_inode.h>
#include <squashfs_block.h>

void	swapendian_xattr_id_table(struct sqsh_xattr_id_table *temp);
void	swapendian_xattr_id(struct sqsh_xattr_id *temp);
void	swapendian_xattr_entry(struct sqsh_xattr_entry *temp);

sqsh_err
sqsh_init_xattr(struct sqsh_mount *ump)
{
	off_t start;
	size_t data_read;

	start  = ump->sb.xattr_id_table_start;
	if (start == SQUASHFS_INVALID_BLK)
		return SQFS_OK;
	data_read = sqsh_io_read_buf(ump, &ump->xattr_info,
		sizeof(ump->xattr_info), start);
	if (data_read != sizeof(ump->xattr_info))
		return SQFS_ERR;
	swapendian_xattr_id_table(&ump->xattr_info);
	return sqsh_init_table(&ump->xattr_table, ump,
		start + sizeof(ump->xattr_info), sizeof(struct sqsh_xattr_id),
		ump->xattr_info.xattr_ids);
}

sqsh_err sqsh_xattr_open(struct sqsh_mount *ump, struct sqsh_inode *inode,
	struct sqsh_xattr *x)
{
	sqfs_err err;

	x->remain = 0;
	if (ump->xattr_info.xattr_ids == 0 || inode->xattr == SQUASHFS_INVALID_XATTR)
		return SQFS_OK;

	err = sqsh_get_table(&ump->xattr_table, ump, inode->xattr,
		&x->info);
	if (err != SQFS_OK)
		return SQFS_ERR;
	swapendian_xattr_id(&x->info);

	sqsh_metadata_run_inode(&x->c_next, x->info.xattr,
		ump->xattr_info.xattr_table_start);

	x->ump = ump;
	x->remain = x->info.count;
	x->cursors = CURS_NEXT;
	return SQFS_OK;
}

sqsh_err
sqsh_xattr_read(struct sqsh_xattr *x)
{
	sqsh_err err;

	if (x->remain == 0)
		return SQFS_ERR;

	if (!(x->cursors & CURS_NEXT)) {
		x->ool = false;
		if ((err = sqsh_xattr_value(x, NULL)))
			return err;
	}

	x->c_name = x->c_next;
	err = sqsh_metadata_get(x->ump, &x->c_name, &x->entry, sizeof(x->entry));
	if (err != SQFS_OK)
		return err;
	swapendian_xattr_entry(&x->entry);

	x->type = x->entry.type & SQUASHFS_XATTR_PREFIX_MASK;
	x->ool = x->entry.type & SQUASHFS_XATTR_VALUE_OOL;
	if (x->type > SQFS_XATTR_PREFIX_MAX)
		return SQFS_ERR;

	--(x->remain);
	x->cursors = 0;
	return err;
}

void
swapendian_xattr_id_table(struct sqsh_xattr_id_table *temp)
{
	temp->xattr_table_start	=	le64toh(temp->xattr_table_start);
	temp->xattr_ids			=	le32toh(temp->xattr_ids);
	temp->unused			=	le32toh(temp->unused);
}

void
swapendian_xattr_id(struct sqsh_xattr_id *temp)
{
	temp->xattr	=	le64toh(temp->xattr);
	temp->count	=	le32toh(temp->count);
	temp->size	=	le32toh(temp->size);
}

void
swapendian_xattr_entry(struct sqsh_xattr_entry *temp)
{
	temp->type	=	le16toh(temp->type);
	temp->size	=	le16toh(temp->size);
}