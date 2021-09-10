#!/bin/bash -e

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install build dependencies of packages which we instrument.

# Enable source repositories in Goobuntu.
if hash goobuntu-config 2> /dev/null
then
  sudo goobuntu-config set include_deb_src true
fi

# TODO(eugenis): find a way to pull the list from the build config.
common_packages="\
atk1.0 \
dee \
dpkg-dev \
freetype \
gobject-introspection \
libappindicator3-1 \
libasound2 \
libatk-bridge2.0-0 \
libatspi2.0-0 \
libavahi-client3 \
libcairo2 \
libcap2 \
libcups2 \
libcurl3-gnutls \
libdbus-1-3 \
libdbus-glib-1-2 \
libdbusmenu \
libdbusmenu-glib4 \
libexpat1 \
libffi6 \
libfontconfig1 \
libgcrypt11 \
libgdk-pixbuf2.0-0 \
libglib2.0-0 \
libgnome-keyring0 \
libgpg-error0 \
libgtk-3-0 \
libgtk2.0-bin \
libidn11 \
libindicator3-7 \
libido3-0.1-0 \
libjasper1 \
libjpeg-turbo8 \
libnspr4 \
libp11-kit0 \
libpci3 \
libpcre3 \
libpixman-1-0 \
libpng12-0 \
librtmp0 \
libsasl2-2 \
libunity9 \
libwayland-client0 \
libx11-6 \
libxau6 \
libxcb1 \
libxcomposite1 \
libxcursor1 \
libxdamage1 \
libxdmcp6 \
libxext6 \
libxfixes3 \
libxi6 \
libxinerama1 \
libxkbcommon0 \
libxrandr2 \
libxrender1 \
libxss1 \
libxtst6 \
nss \
pango1.0 \
pkg-config \
pulseaudio \
udev \
zlib1g \
brltty"

trusty_specific_packages="\
libtasn1-6 \
harfbuzz
libsecret"

ubuntu_release=$(lsb_release -cs)

if test "$ubuntu_release" = "trusty" ; then
  packages="$common_packages $trusty_specific_packages"
fi

# Extra build deps for pulseaudio, which apt-get build-dep may fail to install
# for reasons which are not entirely clear.
sudo apt-get install libltdl3-dev libjson0-dev \
         libsndfile1-dev libspeexdsp-dev libjack0 \
         chrpath -y --force-yes  # Chrpath is required by fix_rpaths.sh.

# Needed for libldap-2.4.2. libldap is not included in the above list because
# one if its dependencies, libgssapi3-heimdal, conflicts with libgssapi-krb5-2,
# required by libcurl. libgssapi3-heimdal isn't required for this build of
# libldap.
sudo apt-get install libsasl2-dev -y --force-yes

sudo apt-get build-dep -y --force-yes $packages

if test "$ubuntu_release" = "trusty" ; then
  # On Trusty, build deps for some of the instrumented packages above conflict
  # with Chromium's build deps. In particular:
  # zlib1g and libffi remove gcc-4.8 in favor of gcc-multilib,
  # libglib2.0-0 removes libelf in favor of libelfg0.
  # We let Chromium's build deps take priority. So, run Chromium's
  # install-build-deps.sh to reinstall those that have been removed.
  $(dirname ${BASH_SOURCE[0]})/../../../build/install-build-deps.sh --no-prompt
fi
