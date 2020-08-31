# Makefile for gperf

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

# Programs used by "make":
RM = rm -f


#### End of system configuration section. ####

SHELL = /bin/sh

all : force
	cd lib; $(MAKE) all
	cd src; $(MAKE) all
	cd tests; $(MAKE) all
	cd doc; $(MAKE) all

install : force
	cd lib; $(MAKE) install
	cd src; $(MAKE) install
	cd tests; $(MAKE) install
	cd doc; $(MAKE) install

installdirs : force
	cd lib; $(MAKE) installdirs
	cd src; $(MAKE) installdirs
	cd tests; $(MAKE) installdirs
	cd doc; $(MAKE) installdirs

uninstall : force
	cd lib; $(MAKE) uninstall
	cd src; $(MAKE) uninstall
	cd tests; $(MAKE) uninstall
	cd doc; $(MAKE) uninstall

check : force
	cd lib; $(MAKE) check
	cd src; $(MAKE) check
	cd tests; $(MAKE) check
	cd doc; $(MAKE) check

mostlyclean : force
	cd lib; $(MAKE) mostlyclean
	cd src; $(MAKE) mostlyclean
	cd tests; $(MAKE) mostlyclean
	cd doc; $(MAKE) mostlyclean

clean : force
	cd lib; $(MAKE) clean
	cd src; $(MAKE) clean
	cd tests; $(MAKE) clean
	cd doc; $(MAKE) clean

distclean : force
	cd lib; if test -f Makefile; then $(MAKE) distclean; fi
	cd src; if test -f Makefile; then $(MAKE) distclean; fi
	cd tests; if test -f Makefile; then $(MAKE) distclean; fi
	cd doc; if test -f Makefile; then $(MAKE) distclean; fi
	$(RM) config.status config.log config.cache Makefile

maintainer-clean : force
	cd lib; if test -f Makefile; then $(MAKE) maintainer-clean; fi
	cd src; if test -f Makefile; then $(MAKE) maintainer-clean; fi
	cd tests; if test -f Makefile; then $(MAKE) maintainer-clean; fi
	cd doc; if test -f Makefile; then $(MAKE) maintainer-clean; fi
	$(RM) config.status config.log config.cache Makefile

force :

