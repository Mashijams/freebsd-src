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

#ifndef SQUASHFS_XATTR_H
#define SQUASHFS_XATTR_H

struct sqsh_prefix {
	const char	*pref;
	size_t		len;
};

struct sqsh_prefix sqsh_xattr_prefixes[] = {
	{"user.", 5},
	{"trusted.", 8},
	{"security.", 9},
};

typedef enum {
	CURS_VSIZE	=	1,
	CURS_VAL	=	2,
	CURS_NEXT	=	4
} sqsh_xattr_curs;

struct sqsh_xattr {
	struct sqsh_mount		*ump;
	int						cursors;
	struct sqsh_block_run	c_name;
	struct sqsh_block_run	c_vsize;
	struct sqsh_block_run	c_val;
	struct sqsh_block_run	c_next;
	size_t					remain;
	struct sqsh_xattr_id	info;
	uint16_t				type;
	bool					ool;
	struct sqsh_xattr_entry	entry;
	struct sqsh_xattr_val	val;
};

sqsh_err sqsh_init_xattr(struct sqsh_mount *ump);

sqsh_err sqsh_xattr_open(struct sqsh_mount *ump, struct sqsh_inode *inode,
			struct sqsh_xattr *x);

sqsh_err sqsh_xattr_read(struct sqsh_xattr *x);

/* Helper functions on sqsh_xattr */
size_t		sqsh_xattr_name_size(struct sqsh_xattr *x);
sqsh_err	sqsh_xattr_name(struct sqsh_xattr *x, char *name, bool prefix);
sqsh_err	sqsh_xattr_value_size(struct sqsh_xattr *x, size_t *size);
sqsh_err	sqsh_xattr_value(struct sqsh_xattr *x, void *buf);

static sqsh_err	sqsh_xattr_find_prefix(const char *name, uint16_t *type);

sqsh_err	sqsh_xattr_find(struct sqsh_xattr *x, const char *name, bool *found);
sqsh_err	sqsh_xattr_lookup(struct sqsh_mount *ump, struct sqsh_inode *inode,
				const char *name, void *buf, size_t *size);

#endif