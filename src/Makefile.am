INCLUDES = 						\
	$(FREETYPE_CFLAGS)				\
	$(EXPAT_CFLAGS)					\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/src
	
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
	-version-number @LT_VERSION_NUMBER@

libfontconfig_la_LIBADD = $(FREETYPE_LIBS) $(EXPAT_LIBS)

