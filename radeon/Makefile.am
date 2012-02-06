# Copyright © 2008 Jérôme Glisse
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# Authors:
#    Jérôme Glisse <glisse@freedesktop.org>

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/radeon \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_radeon_la_LTLIBRARIES = libdrm_radeon.la
libdrm_radeon_ladir = $(libdir)
libdrm_radeon_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_radeon_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_radeon_la_SOURCES = \
	radeon_bo_gem.c \
	radeon_cs_gem.c \
	radeon_cs_space.c \
	radeon_bo.c \
	radeon_cs.c \
	radeon_surface.c \
	bof.c \
	bof.h

libdrm_radeonincludedir = ${includedir}/libdrm
libdrm_radeoninclude_HEADERS = \
	radeon_bo.h \
	radeon_cs.h \
	radeon_surface.h \
	radeon_bo_gem.h \
	radeon_cs_gem.h \
	radeon_bo_int.h \
	radeon_cs_int.h \
	r600_pci_ids.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_radeon.pc

EXTRA_DIST = libdrm_radeon.pc.in
