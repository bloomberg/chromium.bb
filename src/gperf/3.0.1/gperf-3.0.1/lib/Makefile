# Makefile for gperf/lib

# Copyright (C) 1989, 1992, 1993, 1998, 2002 Free Software Foundation, Inc.
# Written by Douglas C. Schmidt <schmidt@ics.uci.edu>
# and Bruno Haible <bruno@clisp.org>.
#
# This file is part of GNU GPERF.
#
# GNU GPERF is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU GPERF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.
# If not, write to the Free Software Foundation, Inc.,
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#### Start of system configuration section. ####

# Directories used by "make":
srcdir = ../../gperf-3.0.1-src/lib

# Directories used by "make install":
prefix = c:/progra~1/Gperf
local_prefix = /usr/local
exec_prefix = ${prefix}

# Programs used by "make":
# C compiler
CC = gcc
CFLAGS =  -Wall -O3 -fms-extensions -mms-bitfields  -fno-exceptions -fomit-frame-pointer -march=i386 -mcpu=i686  
CPP = gcc -E
# C++ compiler
CXX = g++
CXXFLAGS =   -Wall -O3 -fms-extensions -mms-bitfields  -fno-exceptions -fomit-frame-pointer -march=i386 -mcpu=i686    -fconserve-space -fno-rtti  
CXXCPP = g++ -E
# Other
AR = ar
AR_FLAGS = rc
RANLIB = ranlib
MV = mv
LN = ln
RM = rm -f


# Programs used by "make install":
INSTALL = /bin/install.exe -cpD
INSTALL_PROGRAM = /bin/install.exe -cpD
INSTALL_DATA = /bin/install.exe -cp

#### End of system configuration section. ####

SHELL = /bin/sh

VPATH = $(srcdir) $(resdir)

OBJECTS  = getopt.o getopt1.o getline.o hash.o
CPPFLAGS = -I$(srcdir)

TARGETLIB = libgp.a

all : $(TARGETLIB)

$(TARGETLIB): $(OBJECTS)
	$(RM) $@
	$(AR) $(AR_FLAGS) $@ $(OBJECTS)
	$(RANLIB) $@

# Don't use implicit rules, since AIX "make" and OSF/1 "make" don't always
# expand $< correctly in this context.
#
#%.o : %.c
#	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<
#
#%.o : %.cc
#	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

getopt.o : getopt.c getopt.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(srcdir)/getopt.c

getopt1.o : getopt1.c getopt.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(srcdir)/getopt1.c

getline.o : getline.cc getline.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/getline.cc

hash.o : hash.cc hash.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(srcdir)/hash.cc

install : all

installdirs :

uninstall :

check : all

mostlyclean : clean

clean : force
	$(RM) *~ *.s *.o *.a $(TARGETLIB) core

distclean : clean
	$(RM) config.status config.log config.cache Makefile

maintainer-clean : distclean

force :
