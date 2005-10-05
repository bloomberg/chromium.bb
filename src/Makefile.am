#
# $Id$
#
# Copyright © 2003 Keith Packard
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of Keith Packard not be used in
# advertising or publicity pertaining to distribution of the software without
# specific, written prior permission.  Keith Packard makes no
# representations about the suitability of this software for any purpose.  It
# is provided "as is" without express or implied warranty.
#
# KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
# EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
# DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

if OS_WIN32

export_symbols = -export-symbols fontconfig.def

# gcc import library install/uninstall

install-libtool-import-lib: 
	$(INSTALL) .libs/libfontconfig.dll.a $(DESTDIR)$(libdir)
	$(INSTALL) fontconfig.def $(DESTDIR)$(libdir)/fontconfig.def

uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libfontconfig.dll.a $(DESTDIR)$(libdir)/fontconfig.def

else

install-libtool-import-lib:
uninstall-libtool-import-lib:

endif

if MS_LIB_AVAILABLE

# Microsoft import library install/uninstall

noinst_DATA = fontconfig.lib

fontconfig.lib : libfontconfig.la
	lib -name:libfontconfig-$(lt_current_minus_age).dll -def:fontconfig.def -out:$@

install-ms-import-lib:
	$(INSTALL) fontconfig.lib $(DESTDIR)$(libdir)

uninstall-ms-import-lib:
	-rm $(DESTDIR)$(libdir)/fontconfig.lib

else

install-ms-import-lib:
uninstall-ms-import-lib:

endif

INCLUDES = 						\
	$(FREETYPE_CFLAGS)				\
	$(LIBXML2_CFLAGS)				\
	$(EXPAT_CFLAGS)					\
	$(WARN_CFLAGS)					\
	-DFONTCONFIG_PATH='"$(CONFDIR)"'		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/src

EXTRA_DIST = fontconfig.def.in

noinst_HEADERS=fcint.h

libfontconfig_la_SOURCES = \
	fcatomic.c \
	fcblanks.c \
	fccache.c \
	fccfg.c \
	fccharset.c \
	fcdbg.c \
	fcdefault.c \
	fcdir.c \
	fcfreetype.c \
	fcfs.c \
	fcinit.c \
	fclang.c \
	fclist.c \
	fcmatch.c \
	fcmatrix.c \
	fcname.c \
	fcpat.c \
	fcstr.c \
	fcxml.c

lib_LTLIBRARIES = libfontconfig.la

libfontconfig_la_LDFLAGS =			\
	-version-info @LT_VERSION_INFO@ -no-undefined $(export_symbols)

libfontconfig_la_LIBADD = $(FREETYPE_LIBS) $(LIBXML2_LIBS) $(EXPAT_LIBS)

install-data-local: install-ms-import-lib install-libtool-import-lib

uninstall-local: uninstall-ms-import-lib uninstall-libtool-import-lib

