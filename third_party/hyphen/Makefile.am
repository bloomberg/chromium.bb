SUBDIRS= doc tests

lib_LTLIBRARIES = libhyphen.la
libhyphen_la_SOURCES = hnjalloc.c hyphen.c
libhyphen_la_LDFLAGS = -version-info 2:0:2
include_HEADERS = hyphen.h
noinst_HEADERS = hnjalloc.h

noinst_PROGRAMS = substrings example
substrings_SOURCES = substrings.c
example_SOURCES = example.c
example_LDADD = libhyphen.la

bin_SCRIPTS = substrings.pl

hyphdir = $(datadir)/hyphen

# use it if you can recreate hyph_en_US.dic from
hyph_DATA = hyph_en_US.dic

EXTRA_DIST = checkme.lst hyphen.tex tbhyphext.tex tbhyphext.sh \
    README.compound README.hyphen README.nonstandard substrings.pl hyphen.patch \
    COPYING.MPL COPYING.LGPL hyph_en_US.dic README_hyph_en_US.txt lig.awk \
    ligpatch.txt ooopatch.sed

hyphen.us3:
	cp -f $(srcdir)/hyphen.tex hyphen.us
	patch < $(srcdir)/hyphen.patch
	$(srcdir)/tbhyphext.sh <$(srcdir)/tbhyphext.tex >hyphen.us2
	cat hyphen.us hyphen.us2 | $(AWK) -f $(srcdir)/lig.awk >hyphen.us3
	cat $(srcdir)/ligpatch.txt >>hyphen.us3

hyph_en_US.dic: hyphen.us3
	perl $(srcdir)/substrings.pl hyphen.us3 hyphen.us4 UTF-8 2 3 >/dev/null
	cat hyphen.us4 | $(SED) -f $(srcdir)/ooopatch.sed >hyph_en_US.dic

clean-local:
	rm -rf hyphen.us* hyph_en_US.dic
