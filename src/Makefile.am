if OS_WIN32

no_undefined = -no-undefined
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
	$(EXPAT_CFLAGS)					\
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
	-version-info @LT_VERSION_INFO@ $(no_undefined) $(export_symbols)

libfontconfig_la_LIBADD = $(FREETYPE_LIBS) $(EXPAT_LIBS)

install-data-local: install-ms-import-lib install-libtool-import-lib

uninstall-local: uninstall-ms-import-lib uninstall-libtool-import-lib

