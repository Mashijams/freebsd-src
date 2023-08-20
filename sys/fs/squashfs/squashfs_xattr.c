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

static	MALLOC_DEFINE(M_SQUASHFSEXT, "SQUASHFS xattrs", "SQUASHFS Extended attributes");

void	swapendian_xattr_id_table(struct sqsh_xattr_id_table *temp);
void	swapendian_xattr_id(struct sqsh_xattr_id *temp);
void	swapendian_xattr_entry(struct sqsh_xattr_entry *temp);
void	swapendian_xattr_value(struct sqsh_xattr_val *temp)

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
		err = sqsh_xattr_value(x, NULL);
		if (err != SQFS_OK)
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

size_t
sqsh_xattr_name_size(struct sqsh_xattr *x)
{
	return x->entry.size + sqsh_xattr_prefixes[x->type].len;
}

sqsh_err
sqsh_xattr_name(struct sqsh_xattr *x, char *name, bool prefix)
{
	sqsh_err err;

	if (name && prefix) {
		struct sqsh_prefix *p = &sqsh_xattr_prefixes[x->type];
		memcpy(name, p->pref, p->len);
		name += p->len;
	}

	x->c_vsize = x->c_name;
	err = sqsh_metadata_get(x->ump, &x->c_vsize, name, x->entry.size);
	if (err != SQFS_OK)
		return err;

	x->cursors |= CURS_VSIZE;
	return err;
}

sqsh_err
sqsh_xattr_value_size(struct sqsh_xattr *x, size_t *size)
{
	sqsh_err err;

	if (!(x->cursors & CURS_VSIZE)) {
		err = sqsh_xattr_name(x, NULL, false);
		if (err != SQFS_OK)
			return err;
	}

	x->c_val = x->c_vsize;
	err = sqsh_metadata_get(x->ump, &x->c_val, &x->val, sizeof(x->val));
	if (err != SQFS_OK)
		return err;
	swapendian_xattr_value(&x->val);

	if (x->ool) {
		uint64_t pos;
		x->c_next = x->c_val;
		err = sqsh_metadata_get(x->ump, &x->c_next, &pos, sizeof(pos))
		if (err != SQFS_OK)
			return err;
		pos = le64toh(pos);
		x->cursors |= CURS_NEXT;

		sqsh_metadata_run_inode(&x->c_val, pos,
			x->ump->xattr_info.xattr_table_start);
		err = sqsh_metadata_get(x->ump, &x->c_val, &x->val, sizeof(x->val))
		if (err != SQFS_OK)
			return err;
		swapendian_xattr_value(&x->val);
	}

	if (size)
		*size = x->val.vsize;
	x->cursors |= CURS_VAL;
	return err;
}

sqsh_err
sqsh_xattr_value(struct sqsh_xattr *x, void *buf)
{
	sqsh_err err;
	struct sqsh_block_run c;

	if (!(x->cursors & CURS_VAL)) {
		err = sqsh_xattr_value_size(x, NULL);
		if (err != SQFS_OK)
			return err;
	}

	c = x->c_val;
	err = sqsh_metadata_get(x->ump, &c, buf, x->val.vsize);
	if (err != SQFS_OK)
		return err;

	if (!x->ool) {
		x->c_next = c;
		x->cursors |= CURS_NEXT;
	}
	return err;
}

static sqsh_err
sqsh_xattr_find_prefix(const char *name, uint16_t *type)
{
	int i;
	for (i = 0; i <= SQFS_XATTR_PREFIX_MAX; ++i) {
		struct sqsh_prefix *p = &sqsh_xattr_prefixes[i];
		if (strncmp(name, p->pref, p->len) == 0) {
			*type = i;
			return SQFS_OK;
		}
	}
	return SQFS_ERR;
}

sqsh_err
sqsh_xattr_find(struct sqsh_xattr *x, const char *name, bool *found)
{
	sqsh_err err;
	char *cmp = NULL;
	uint16_t type;
	size_t len;

	err = sqsh_xattr_find_prefix(name, &type);

	if (err != SQFS_OK) {
		*found = false;
		return SQFS_OK;
	}

	name += sqsh_xattr_prefixes[type].len;
	len = strlen(name);
	cmp = malloc(len, M_SQUASHFSEXT, M_WAITOK | M_ZERO);

	while (x->remain) {
		err = sqsh_xattr_read(x);
		if (err != SQFS_OK)
			goto done;
		if (x->type != type && x->entry.size != len)
			continue;
		err = sqsh_xattr_name(x, cmp, false);
		if (err != SQFS_OK)
			goto done;
		if (strncmp(name, cmp, len) == 0) {
			*found = true;
			goto done;
		}
	}
	*found = false;

done:
	free(cmp, M_SQUASHFSEXT);
	return err;
}

sqsh_err
sqsh_xattr_lookup(struct sqsh_mount *ump, struct sqsh_inode *inode,
	const char *name, void *buf, size_t *size)
{
	sqsh_err err;
	bool found;

	struct sqsh_xattr xattr;
	err = sqsh_xattr_open(ump, inode, &xattr);
	if (err != SQFS_OK)
		return err;

	found = false;
	err = sqsh_xattr_find(&xattr, name, &found);
	if (err != SQFS_OK)
		return err;
	if (!found) {
		*size = 0;
		return err;
	}

	size_t real;
	err = sqsh_xattr_value_size(&xattr, &real);
	if (err != SQFS_OK)
		return err;

	if (buf && *size >= real) {
		err = sqsh_xattr_value(&xattr, buf);
		if (err != SQFS_OK)
			return err;
	}

	*size = real;
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

void
swapendian_xattr_value(struct sqsh_xattr_val *temp)
{
	temp->vsize	=	le32toh(temp->vsize);
}