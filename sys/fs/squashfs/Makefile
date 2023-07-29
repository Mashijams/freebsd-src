KMOD=	squashfs
SRCS=	\
	squashfs_vfsops.c \
	squashfs_vnops.c \
	squashfs_decompressor.c \
	squashfs_block.c \
	squashfs_inode.c \
	squashfs_io.c

SRCS+=	vnode_if.h
SRCS+=	opt_gzio.h
SRCS+=	opt_zstdio.h

.include <bsd.kmod.mk>
