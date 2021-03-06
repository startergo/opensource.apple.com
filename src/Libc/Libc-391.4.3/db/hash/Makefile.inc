#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.5 2002/11/18 09:50:54 ru Exp $

.PATH: ${.CURDIR}/db/hash

.include "Makefile.fbsd_begin"
FBSDMISRCS= hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
FBSDHDRS= hash.h page.h
.include "Makefile.fbsd_end"

# need to rename extern.h to make it unique
${SYMROOT}/hash_extern.h: ${.CURDIR}/db/hash/FreeBSD/extern.h _AUTOPATCHSYM
AUTOPATCHHDRS+= ${SYMROOT}/hash_extern.h
