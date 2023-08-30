KMOD=	squashfs
SRCS=	\
	squashfs_vfsops.c \
	squashfs_vnops.c \
	squashfs_decompressor.c \
	squashfs_block.c \
	squashfs_inode.c \
	squashfs_io.c	\
	squashfs_dir.c	\
	squashfs_file.c	\
	squashfs_xattr.c

.if !defined(KERNBUILDDIR)
CFLAGS+=	-DZSTDIO
CFLAGS+=	-DGZIO
.endif

SRCS+=	vnode_if.h
SRCS+=	opt_gzio.h
SRCS+=	opt_zstdio.h

CFLAGS+=	-I${SRCTOP}/sys/contrib/zstd/lib/freebsd
CFLAGS+=	-I${SRCTOP}/sys/contrib/zlib/lib/freebsd

.include <bsd.kmod.mk>
