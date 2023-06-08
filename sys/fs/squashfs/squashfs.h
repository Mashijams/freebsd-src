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

#ifndef SQUASHFS_H
#define SQUASHFS_H

#define SQUASHFS_MAGIC				0x73717368
#define SQUASHFS_MAGIC_SWAP 		0x68737173

#define SQUASHFS_CACHED_FRAGMENTS	3
#define SQUASHFS_MAJOR				4
#define SQUASHFS_MINOR				0
#define SQUASHFS_START				0


#define SQUASHFS_METADATA_SIZE		8192
#define SQUASHFS_METADATA_LOG		13

#define SQUASHFS_FILE_SIZE			131072
#define SQUASHFS_FILE_LOG			17

#define SQUASHFS_FILE_MAX_SIZE		1048576
#define SQUASHFS_FILE_MAX_LOG		20

#define SQUASHFS_IDS				65536

#define SQUASHFS_MAX_NAME_LEN		256

#define SQUASHFS_INVALID_FRAG		(0xffffffffU)
#define SQUASHFS_INVALID_XATTR		(0xffffffffU)
#define SQUASHFS_INVALID_BLK		((int64_t)-1)

// Filesystem flags
#define SQUASHFS_NOI				0
#define SQUASHFS_NOD				1
#define SQUASHFS_NOF				3
#define SQUASHFS_NO_FRAG			4
#define SQUASHFS_ALWAYS_FRAG		5
#define SQUASHFS_DUPLICATE			6
#define SQUASHFS_EXPORT				7
#define SQUASHFS_COMP_OPT			10

// Max number of types and file types
#define SQUASHFS_DIR_TYPE			1
#define SQUASHFS_REG_TYPE			2
#define SQUASHFS_SYMLINK_TYPE		3
#define SQUASHFS_BLKDEV_TYPE		4
#define SQUASHFS_CHRDEV_TYPE		5
#define SQUASHFS_FIFO_TYPE			6
#define SQUASHFS_SOCKET_TYPE		7
#define SQUASHFS_LDIR_TYPE			8
#define SQUASHFS_LREG_TYPE			9
#define SQUASHFS_LSYMLINK_TYPE		10
#define SQUASHFS_LBLKDEV_TYPE		11
#define SQUASHFS_LCHRDEV_TYPE		12
#define SQUASHFS_LFIFO_TYPE			13
#define SQUASHFS_LSOCKET_TYPE		14

#define SQUASHFS_COMPRESSED_BIT		(1 << 15)
#define SQUASHFS_COMPRESSED_BIT_BLOCK	(1 << 24)

// cached data constants for filesystem
#define SQUASHFS_CACHED_BLKS		8

#define SQUASHFS_MAX_FILE_SIZE_LOG	64

#define SQUASHFS_MAX_FILE_SIZE		(1LL << \
					(SQUASHFS_MAX_FILE_SIZE_LOG - 2))

// meta index cache
#define SQUASHFS_META_INDEXES		(SQUASHFS_METADATA_SIZE / sizeof(unsigned int))
#define SQUASHFS_META_ENTRIES		127
#define SQUASHFS_META_SLOTS			8

// definitions for structures on disk
#define ZLIB_COMPRESSION	1
#define LZMA_COMPRESSION	2
#define LZO_COMPRESSION		3
#define XZ_COMPRESSION		4
#define LZ4_COMPRESSION		5
#define ZSTD_COMPRESSION	6

struct sqsh_sb {
	uint32_t		s_magic;
	uint32_t		inodes;
	uint32_t		mkfs_time;
	uint32_t		block_size;
	uint32_t		fragments;
	uint16_t		compression;
	uint16_t		block_log;
	uint16_t		flags;
	uint16_t		no_ids;
	uint16_t		s_major;
	uint16_t		s_minor;
	uint64_t		root_inode;
	uint64_t		bytes_used;
	uint64_t		id_table_start;
	uint64_t		xattr_id_table_start;
	uint64_t		inode_table_start;
	uint64_t		directory_table_start;
	uint64_t		fragment_table_start;
	uint64_t		lookup_table_start;
};

struct sqsh_dir_index {
	uint32_t		index;
	uint32_t		start_block;
	uint32_t		size;
};

struct sqsh_base_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
};

struct sqsh_ipc_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
};

struct sqsh_lipc_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
	uint32_t		xattr;
};

struct sqsh_dev_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
	uint32_t		rdev;
};

struct sqsh_ldev_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
	uint32_t		rdev;
	uint32_t		xattr;
};

struct sqsh_symlink_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
	uint32_t		symlink_size;
};

struct sqsh_reg_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		start_block;
	uint32_t		fragment;
	uint32_t		offset;
	uint32_t		file_size;
};

struct sqsh_lreg_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint64_t		start_block;
	uint64_t		file_size;
	uint64_t		sparse;
	uint32_t		nlink;
	uint32_t		fragment;
	uint32_t		offset;
	uint32_t		xattr;
};

struct sqsh_dir_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		start_block;
	uint32_t		nlink;
	uint16_t		file_size;
	uint16_t		offset;
	uint32_t		parent_inode;
};

struct sqsh_ldir_inode {
	uint16_t		inode_type;
	uint16_t		mode;
	uint16_t		uid;
	uint16_t		guid;
	uint32_t		mtime;
	uint32_t		inode_number;
	uint32_t		nlink;
	uint32_t		file_size;
	uint32_t		start_block;
	uint32_t		parent_inode;
	uint16_t		i_count;
	uint16_t		offset;
	uint32_t		xattr;
};

struct sqsh_dir_entry {
	uint16_t		offset;
	uint16_t		inode_number;
	uint16_t		type;
	uint16_t		size;
};

struct sqsh_dir_header {
	uint32_t		count;
	uint32_t		start_block;
	uint32_t		inode_number;
};

#ifdef SQUASHFS_DEBUG
#define DEBUG(x...)	printf("\n\033[0;34msquashfs:\33[0m " x)
#else
#define DEBUG(x...)
#endif // SQUASHFS_DEBUG
#define ERROR(x...)	printf("\n\033[0;31msquashfs:\33[0m " x)

typedef enum {
	SQFS_OK,			// everything fine
	SQFS_BADFORMAT,		// unsupported file format
	SQFS_BADVERSION,	// unsupported squashfs version
	SQFS_BADCOMP,		// unsupported compression method
	SQFS_UNSUP,			// unsupported feature
	SQFS_ERR			// error in operation
} sqsh_err;

#endif // SQUASHFS_H