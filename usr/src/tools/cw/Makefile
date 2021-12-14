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

SUBDIRS= aarch64 i386 sparc

install := TARGET = install
all	:= TARGET = all
clobber := TARGET = clobber
clean	:= TARGET = clean
check	:= TARGET = check

install all clean clobber check: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) TARG_MACH=$@ $(TARGET)

FRC:

MAN1FILES=	cw.1onbld

include ../Makefile.tools

$(ROOTONBLDMAN1FILES) := FILEMODE= 644

install: $(ROOTONBLDMAN1FILES)  $(ROOTONBLDBINMACH)/cw

$(ROOTONBLDBINMACH)/cw: FRC
	$(RM) $@; $(SYMLINK) cw.$(NATIVE_MACH) $@

include ../Makefile.targ
