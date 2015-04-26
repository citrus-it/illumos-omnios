#
# Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
#

PROG=	krrpadm

CMNDIR=     $(SRC)/common/krrp

OBJS_KRRP_TOOL = krrpadm.o

OBJS= ${OBJS_KRRP_TOOL}
SRCS=   ${OBJS_KRRP_TOOL:.o=.c}

include ../Makefile.cmd
include ../Makefile.ctf

INCS += -I../../common/krrp

CPPFLAGS += -D_REENTRANT
CPPFLAGS += $(INCS)

LDLIBS += -lumem -lnvpair -lsysevent -lkrrp -luuid

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o: $(CMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

install: all $(ROOTUSRSBINPROG)

clean:
	$(RM) $(OBJS)

_msg:

lint:	lint_SRCS

include ../Makefile.targ
