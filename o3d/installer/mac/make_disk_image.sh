#!/bin/bash
#  Makes O3D disk image file.
#  Syntax: make_disk_image version_string
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

#  Make disk image by copying all the bits we need to a new directory and then
#  calling hdiutil to make a dmg of it.
#  Obviously, this should run after the script that makes the installer.

O3D_INTERNAL_DIR="${PROJECT_DIR}/../../../o3d-internal"
#  If the o3d-internal depot is present, make disk image.
if [ -d "${O3D_INTERNAL_DIR}" ]
then
  MAC_INSTALLER_DIR="${O3D_INTERNAL_DIR}/mac_installer"
  if [ -d "${MAC_INSTALLER_DIR}" ]
  then
    #  Make disk image dir
    IMG_DIR="${BUILT_PRODUCTS_DIR}/DMG_SRC"
    #  Delete any existing directory so we can start from scratch.
    rm -rf "${IMG_DIR}"
    mkdir "${IMG_DIR}"

    if [ -d "${IMG_DIR}" ]
    then
      #  Delete existing dmg if present.
      rm -f "${BUILT_PRODUCTS_DIR}/o3d.dmg"

      #  Get keystone auto update script.
      cp "${MAC_INSTALLER_DIR}/.keystone_install" "${IMG_DIR}"

      #  Get O3D install package.
      cp -R "${BUILT_PRODUCTS_DIR}/O3D.mpkg" "${IMG_DIR}"

      #  Make disk image from the folder we just created.
      hdiutil create -srcfolder "${IMG_DIR}" \
          -size 30mb -ov -fs HFS+ -imagekey zlib-level=9 \
          -volname "O3D ${1}" \
          "${BUILT_PRODUCTS_DIR}/o3d.dmg"

      #  Delete source folder now we are done.
      rm -rf "${IMG_DIR}"

      echo "Mac disk image built"
    else
      echo Could not create dir "${IMG_DIR}".
    fi
  else
    echo Mac installer directory not found.
  fi
else
  echo External build - not making disk image.
fi

