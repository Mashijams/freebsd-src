KMOD=	squashfs
SRCS=	\
	squashfs_vfsops.c \
	squashfs_vnops.c \
	squashfs_decompressor.c \
	squashfs_block.c \
	squashfs_inode.c \
	squashfs_io.c	\
	squashfs_dir.c	\
	squashfs_file.c

SRCS+=	vnode_if.h
CFLAGS+= -I/usr/include
CFLAGS+= -I/usr/local/include

.include <bsd.kmod.mk>
