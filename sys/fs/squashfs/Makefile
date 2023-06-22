KMOD=	squashfs
SRCS=	\
	squashfs_vfsops.c \
	squashfs_vnops.c \
	squashfs_decompressor.c

SRCS+=	vnode_if.h

CFLAGS+= -I/usr/include
CFLAGS+= -I/usr/local/include

.include <bsd.kmod.mk>
