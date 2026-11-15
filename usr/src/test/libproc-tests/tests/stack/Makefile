#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2026 Oxide Computer Company
#

PROGS = \
	stack_iter

SCRIPTS = \
	stack_iter

TESTDIR = stack

include $(SRC)/test/libproc-tests/Makefile.com

CSTD = $(CSTD_GNU17)

stack_iter.32 := LDLIBS += -lproc
stack_iter.64 := LDLIBS64 += -lproc

#
# The target is only built as a 32-bit program. It includes functions which
# are written in assembly so that the layout of their stack frames is known.
#
TARGET = stack_target.32
TARGET_OBJS = stack_target.o.32 stack_frames.o.32

ASFLAGS += -D_ASM

all: $(TARGET)

install: $(ROOTOPTTARG)/$(TARGET)

clobber: clobber_target

clobber_target:
	-$(RM) $(TARGET)

$(TARGET): $(TARGET_OBJS)
	$(LINK.c) -o $@ $(TARGET_OBJS) $(LDLIBS)
	$(POST_PROCESS)

stack_frames.o.32: stack_frames.S
	$(COMPILE.s) -o $@ stack_frames.S
