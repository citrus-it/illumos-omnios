MK_ARCHIVE?=	no
MK_PROFILE?=	no
MK_PICLIB?=	no

.if ${MACHINE} == "i386"
LIBDIR?=	${libprefix}/lib/i386
CFLAGS+=	-m32
LDFLAGS+=	-m32
AFLAGS+=	-m32
.endif
