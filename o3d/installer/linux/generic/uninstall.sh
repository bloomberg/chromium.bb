#!/bin/sh
# Uninstalls O3D plugin for Linux
#
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following disclaimer
#       in the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
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

PATH=$PATH:/usr/bin/:/usr/local/bin

CheckArch() {
  # Look for 32 or 64 bit arch from the kernel. If we can not get a read
  # on the arch, we'll go with "unknown".
  ARCH=$(uname -m)
  case $ARCH in
    i?86 | i86*)
      ARCH="32bit";;
    x86_64 | amd64)
      ARCH="64bit";;
    *)
      ARCH="unknown";;
  esac

  echo "System architecture mapped to: $ARCH"
}


SetRootUser() {
  # We need to be root (or run as sudo) in order to perform uninstall.
  USER=$(id -u)
  if [ "$USER" != "0" ]; then
    echo "You must be root (or sudo) to remove this package."
    exit 1
  fi
}


Uninstall() {
  # Remove O3D plugin, dirs and symlinks.
  O3D_DIR="/opt/google/o3d"
  APPS="mozilla firefox iceape iceweasel xulrunner midbrowser xulrunner-addons"
  LIBS="libCg.so libCgGL.so"
  LIB3D="libnpo3dautoplugin.so"
  LIB3D_WRAPPED="npwrapper.libnpo3dautoplugin.so"

  # Prompt user for verification to remove files and directories.
  echo "
  Do you wish to completely remove all previous version(s) of
  the O3D plugin, directories and included libs? (y/N)"
  read answer
  if [ "$answer" = "y" ]; then
    echo -n "Removing O3D plugin and directories..."
    rm ${O3D_DIR}/libnpo3dautoplugin.so
    rm ${O3D_DIR}/lib/libCg.so
    rm ${O3D_DIR}/lib/libCgGL.so
    for app in $APPS; do
      if [ -L "/usr/lib/${app}/plugins/${LIB3D}" ]; then
        rm /usr/lib/${app}/plugins/${LIB3D}
      fi
    done
    rm ${O3D_DIR}/uninstall.sh
    rmdir $O3D_DIR
    echo "ok"
  else
    echo "Aborting at users request"
    exit 0
  fi

  # If 64bit, remove the wrapped plugin.
  if [ "$ARCH" = "64bit" ]; then
    echo -n "Removing $LIB3D_WRAPPED..."
    for app in $APPS; do
      if [ -L "/usr/lib/${app}/plugins/${LIB3D_WRAPPED}" ]; then
        rm /usr/lib/${app}/plugins/${LIB3D_WRAPPED}
      fi
    done
    echo "Done. Uninstall complete!"
    exit 0
  fi

  echo "Done. Uninstall complete!"
  exit 0
}

CheckArch
SetRootUser
Uninstall
