#!/bin/bash
#  Makes the O3D installer.
#  Syntax: make_installer version_string
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

#  Make the installer by copying any missing raw materials to the built products
#  directory and then invoking "freeze" on the installer project file.
#  Does not currently use the passed-in version string.

O3D_INTERNAL_DIR="${PROJECT_DIR}/../../../o3d-internal"
#  If the o3d-internal depot is present, make installer.
if [ -d "${O3D_INTERNAL_DIR}" ]
then
  MAC_INSTALLER_DIR="${O3D_INTERNAL_DIR}/mac_installer"
  if [ -d "${MAC_INSTALLER_DIR}" ]
  then
    #  Get the installer project.
    cp -f "${MAC_INSTALLER_DIR}/o3d.packproj" \
        "${BUILT_PRODUCTS_DIR}"

    # Get the keystone post install script.
    cp -f "${MAC_INSTALLER_DIR}/postflight.sh" \
        "${BUILT_PRODUCTS_DIR}"

    #  Get keystone.
    cp -R -f  "${MAC_INSTALLER_DIR}/GoogleSoftwareUpdate.pkg" \
        "${BUILT_PRODUCTS_DIR}"

    #  Get the installer plug-in which asks about stats collection.
    cp -R -f  "${MAC_INSTALLER_DIR}/O3D_Stats.bundle" \
        "${BUILT_PRODUCTS_DIR}"

    #  Now we have everything in-place, make the installer.
    /usr/local/bin/freeze "${BUILT_PRODUCTS_DIR}/o3d.packproj"

    echo Mac installer built.
  else
    echo Mac installer directory not found.
  fi
else
  echo External build - not making installer.
fi

