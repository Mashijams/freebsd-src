KMOD=	squashfs
SRCS=	\
	squashfs_block.c \
	squashfs_io.c \
	squashfs_decompressor.c \
	squashfs_inode.c

SRCS+=	vnode_if.h
SRCS+=	opt_gzio.h
SRCS+=	opt_zstdio.h

.include <bsd.kmod.mk>
