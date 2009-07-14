#
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Makefile for fetching and installing ImageMagick third party library
# into the thid_party/nacl_sdk directory.
#

# WARNING: this Makefile has only been verified to work in Linux.

# PRISTINE/BUILD/MODIFIED subdirectories:
#
# PRISTINE contains the version downloaded from imagemagick.org,
# unchanged.  This is used as a reference when creating the recursive
# diff (patch input file).
#
# MODIFIED contains the version being worked on.  This is needed only
# during development and used as the new source subdirectory for
# creating the recursive diff (patch input).
#
# BUILD is a copy of the PRISTINE source, with the diff applied.  This
# is where the configuration and compilation actually occurs.
#
# .ts files are timestamp files used to note that an expensive
# operation has been completed.  these are zero-length files used
# solely for make's timestamp comparisons.

VERSION=6.4.5

#BASE=$(shell p4 client -o | sed -n 's/^Root:[ \t]*//p')
BASE=$(shell pwd | sed 's,/googleclient/native_client/third_party/ImageMagick,,')
GOOGLECLIENT=$(BASE)/googleclient
NACL=$(GOOGLECLIENT)/native_client
NACL_SDK_BASE=$(GOOGLECLIENT)/third_party/nacl_sdk
UNAME=$(shell uname)

# all:	apply_patch config

# apply_patch rule must fire before config, but we don't want
# config to always invoke apply_patch (which fires clean_build)

all:	apply_patch
	$(MAKE) $(MAKEFLAGS) config
	$(MAKE) $(MAKEFLAGS) build

ifeq ($(UNAME),Linux)
SDK=$(NACL_SDK_BASE)/linux/sdk/nacl-sdk/bin
NACL_PATH=$(NACL_SDK_BASE)/linux/sdk/nacl-sdk/nacl
# NACL_PATH=/usr/local/nacl-sdk/nacl
platform:

else
ifeq ($(UNAME),Darwin)
SDK=$(NACL_SDK_BASE)/mac/sdk/nacl-sdk/bin
NACL_PATH=$(NACL_SDK_BASE)/linux/sdk/nacl-sdk/nacl
platform:

else
platform:
	@echo "Unknown platform" >&2; exit 1
endif
endif


# fetch the tarball from the ImageMagic site.  Replace an existing
# tarball, if any, only if the tarballs differ.  This means we can
# rely on the timestamp on the tarball to prevent rebuilding from
# scratch every time.
#
# Note, however, that grabbing the tarball itself is still pretty
# expensive....
fetch:
	curl -s -o ImageMagick.tar..gz ftp://ftp.imagemagick.org/pub/ImageMagick/ImageMagick.tar.gz
	if [ -f ImageMagick.tar.gz ] && cmp -s ImageMagick.tar..gz ImageMagick.tar.gz; then echo "ImageMagick tarball unchanged"; rm ImageMagick.tar..gz; else echo "New ImageMagick tarball"; mv ImageMagick.tar..gz ImageMagick.tar.gz; fi

# The commented dependency rule would omit the check that there isn't
# a new tarball before checking to see if untarring is needed based on
# the timestamp file and the tarball's modification times.  Useful for
# debugging, since it decouple the fetch because that's expensive;
# BEWARE if you re-enable it.

# untar:	ImageMagick-$(VERSION).ts
untar:	fetch ImageMagick-$(VERSION).ts

ImageMagick-$(VERSION).ts:	ImageMagick.tar.gz
	V=$$(tar ztf ImageMagick.tar.gz | sed 's,/.*,,;s/^ImageMagick-//' | sort -u); if [ $$V != $(VERSION) ]; then echo "Version mismatch: got %%V, expected $(VERSION)" >&2; exit 1; fi
	mkdir -p PRISTINE
	(cd PRISTINE; tar xpzf ../ImageMagick.tar.gz)
	touch -r ImageMagick.tar.gz ImageMagick-$(VERSION).ts

# Make sure we start with a pristine copy...
#
clean_build:	untar
	mkdir -p BUILD/$(UNAME)
	rm -fr BUILD/$(UNAME)/ImageMagick-$(VERSION) BUILD/$(UNAME)/ImageMagick-$(VERSION)-config.ts
	cp -pr PRISTINE/ImageMagick-$(VERSION) BUILD/$(UNAME)/ImageMagick-$(VERSION)

apply_patch:	clean_build
	(cd BUILD/$(UNAME)/ImageMagick-$(VERSION); patch -p2 < ../../../ImageMagick-$(VERSION).patch)

config:	BUILD/$(UNAME)/ImageMagick-$(VERSION)-config.ts

BUILD/$(UNAME)/ImageMagick-$(VERSION)-config.ts: ImageMagick-$(VERSION).ts
	(cd BUILD/$(UNAME)/ImageMagick-$(VERSION); PATH=$(SDK):$$PATH ./configure --host nacl --build i386-pc-linux-gnu --prefix=$(NACL_PATH) --with-x=no)
	touch BUILD/$(UNAME)/ImageMagick-$(VERSION)-config.ts

# the targets for the build recursive make in ImageMagick is a bit
# hacky / incomplete -- we pick out internal targets from the
# ImageMagick Makefile, which might not work across versions, and
# furthermore some additional building may occur in the install
# target; there are no convenient make targets in the
# ImageMagick-supplied Makefile that omits the binaries.
build:
	( cd BUILD/$(UNAME)/ImageMagick-$(VERSION); PATH=$(SDK):$$PATH make CFLAGS='-DSSIZE_MAX="((ssize_t)(~((size_t)0)>>1))"'  magick/libMagickCore.la Magick++/lib/libMagick++.la wand/libMagickWand.la )

install:	all
	$(MAKE) $(MAKEFLAGS) rec-install

rec-install:
	( cd BUILD/$(UNAME)/ImageMagick-$(VERSION); PATH=$(SDK):$$PATH make CFLAGS='-DSSIZE_MAX="((ssize_t)(~((size_t)0)>>1))"' install-libLTLIBRARIES install-data-am )

# install2 does not do a clean_build and assumes that the patch has
# been applied.  since we are configuring for nacl, configuring and
# building on two platforms shouldn't be necessary (except for
# testing).
install2:
	$(MAKE) $(MAKEFLAGS) config
	$(MAKE) $(MAKEFLAGS) build
	$(MAKE) $(MAKEFLAGS) rec-install

uninstall:
	( cd BUILD/$(UNAME)/ImageMagick-$(VERSION); PATH=$(SDK):$$PATH make CFLAGS='-DSSIZE_MAX="((ssize_t)(~((size_t)0)>>1))"' uninstall )

patch:
	-diff -Naur PRISTINE/ImageMagick-$(VERSION) MODIFIED/ImageMagick-$(VERSION) > ImageMagick-$(VERSION).patch

clean:
	make -C BUILD/$(UNAME)/ImageMagick-$(VERSION) clean

clean-ts:
	rm -f ImageMagick-$(VERSION).ts BUILD/$(UNAME)/ImageMagick-$(VERSION)-config.ts

.PHONY:	fetch untar build clean_build apply_patch config install uninstall patch clean clean-ts rec-install install2
