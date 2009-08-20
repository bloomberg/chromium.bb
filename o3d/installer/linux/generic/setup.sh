#!/bin/sh
# Installs O3D plugin for Linux.
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
  # We need to be root (or run as sudo) in order to perform setup.
  USER=$(id -u)
  if [ "$USER" != "0" ]; then
    echo "You must be root (or sudo) to install this package."
    exit 1
  fi
}


SetupO3d() {
  # Create npapi plugin directories, copy and symlink libs.
  O3D_DIR="/opt/google/o3d"

  PLUGIN_DIRS="/usr/lib/firefox/plugins
               /usr/lib/iceape/plugins
               /usr/lib/iceweasel/plugins
               /usr/lib/midbrowser/plugins
               /usr/lib/mozilla/plugins
               /usr/lib/xulrunner/plugins
               /usr/lib/xulrunner-addons/plugins"

  LIBS="libCg.so
        libCgGL.so
        libGLEW.so.1.5"

  LIB3D="libnpo3dautoplugin.so"

  echo -n "Creating plugin directories..."
  mkdir -p $PLUGIN_DIRS $O3D_DIR
  echo "ok"

  echo -n "Installing files to $O3D_DIR..."
  install --mode=644 ${LIB3D} $O3D_DIR
  install --mode=644 ${LIBS} $O3D_DIR
  echo "ok"

  echo -n "Creating symlinks to plugin..."
  for dir in $PLUGIN_DIRS; do
    ln -sf ${O3D_DIR}/${LIB3D} ${dir}/
  done
  echo "ok"

  # If 32bit arch, use /usr/lib. If 64bit, use /usr/lib32
  if [ "$ARCH" = "32bit" ]; then
    LIBDIR="/usr/lib"
  elif [ "$ARCH" = "64bit" ]; then
    LIBDIR="/usr/lib32"
    NP_WRAP="yes"
  else
    echo "$ARCH not recognized"
    exit 1
  fi

  echo -n "Creating symlinks to libs..."
  mkdir -p $LIBDIR
  for lib in $LIBS; do
    if [ -e "${LIBDIR}/${lib}" ]; then
      echo "$lib already exists, not replacing."
    else
      ln -s ${O3D_DIR}/${lib} ${LIBDIR}/
    fi
  done
  echo "ok"

  # 64bit only: Check for nspluginwrapper, wrap libnpo3dautoplugin.so if found.
  if [ "$NP_WRAP" = "yes" ]; then
    echo -n "Attempting to wrap $LIB3D via nspluginwrapper..."
    NSPW=$(which nspluginwrapper)
    if [ -z "$NSPW" ]; then
      echo "
      nspluginwrapper not found. Without nspluginwrapper you will be
      unable to use O3D in 64-bit browsers. Continue installation? (y/N)"
      read answer
      if [ "$answer" = "y" ]; then
        echo "Ok, Installation complete."
        exit 0
      else
        echo "Aborted install at user request."
        exit 1
      fi
    fi
    WRAPPED_PLUGIN_PATH="/usr/lib/mozilla/plugins/npwrapper.libnpo3dautoplugin.so"
    APPS="iceape iceweasel firefox xulrunner midbrowser xulrunner-addons"
    echo -n "Wrapping $LIB3D..."
    $NSPW -i ${O3D_DIR}/${LIB3D}
    echo "ok"
    echo -n "Creating symlinks to npwrapper.libnpo3dautoplugin.so..."
    for app in $APPS; do
      ln -sf $WRAPPED_PLUGIN_PATH /usr/lib/${app}/plugins/
    done
    echo "ok"
  fi

  if [ $? -ne 0 ]; then
    echo "Installation failed"
    exit 1
  else
    echo "Installation completed successfully!"
  fi
}

CheckArch
SetRootUser
SetupO3d
