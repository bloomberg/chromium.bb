/^top_srcdir = /a\
resdir = @top_builddir@res

/^VPATH/s/$/ $(resdir)/

/^\.cc\?\.o\:/i\
RCTOOL_COMPILE = RCTOOL\
# Rule to make compiled resource (Windows)\
.rc.o:\
	windres --include-dir $(resdir) -i $< -o $@\

/^\.*SUFFIXES/s/$/ .rc/
s/^DEFS =/& -DINSTALLDIR=\\"$(prefix)\\" -DENABLE_RELOCATABLE /

s/^\([^A-Z_]*\)_OBJECTS = /& \1-res.o /
s/^\([^A-Z_]*\)_SOURCES = /& \1-res.rc /

/^VERSION =/a\
MAJOR=$(shell echo $(VERSION) | sed -e "s/\\..*$$//")\
MINOR=$(shell echo $(VERSION) | sed -e "s/^[^\\.]*\\.0*\\([0-9]\\+\\).*$$/\\1/")

#s/^\([^_]*\)_*LDFLAGS = /& -Wl,--major-image-version=$(MAJOR) -Wl,--minor-image-version=$(MINOR) /
#s/^\([^_]*\)_*LDADD = /& -Wl,--major-image-version=$(MAJOR) -Wl,--minor-image-version=$(MINOR) /
s/^LDADD = /& -Wl,--major-image-version=$(MAJOR) -Wl,--minor-image-version=$(MINOR) /

s/@LN_S@/cp -fp/g
s/ln -s /cp -fp /g
s/@LN@/cp -fp/g
