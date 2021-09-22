#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source. A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

# Copyright 2022 OmniOS Community Edition (OmniOSce) Association.

PROG= pfauth
OBJS= pfauth.o isapath.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd
include ../Makefile.cmd.64
include ../Makefile.ctf

CSTD= $(CSTD_GNU99)
FILEMODE =	0555

ROOTPROG=	$(ROOTLIB)/$(PROG)

CPPFLAGS += -I../pfexecd
LDFLAGS += $(ZASLR)
LDLIBS += -lbsm -lpam -lsecdb

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

%.o: %.c
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

%.o: ../pfexecd/%.c
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

include ../Makefile.targ
