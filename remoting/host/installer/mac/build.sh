#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script builds the Chrome Remote Desktop installer and packages
# it into a .dmg.  It requires that Iceberg be installed (for 'freeze').

# The Chrome Remote Desktop installer consists of 2 components:
# * Chromoting installer package
# * Keystone (Google auto-update)

error_exit() {
  echo "ERROR - $@" 1>&2;
  exit 1;
}

PKG_DIR=build
PKGPROJ_CHROMOTING='Chromoting.packproj'
PKGPROJ_CRD='ChromeRemoteDesktop.packproj'
PKGPROJ_CRD_UNINSTALLER='ChromeRemoteDesktopUninstaller.packproj'
PKG_CHROMOTING='Chromoting.pkg'
PKG_CRD='Chrome Remote Desktop.mpkg'

DMG_TEMP=dmg_tmp
DMG_NAME='Chrome Remote Desktop'
DMG_DIR="$DMG_TEMP/$DMG_NAME"
DMG_FILENAME='Chrome Remote Desktop.dmg'

# Clean out previous build.
rm -rf "$PKG_DIR"
rm -f "$DMG_FILENAME"
rm -rf "$DMG_TEMP"   # In case previous build failed.

# Copy latest release build.
# TODO(garykac): Get from proper location.
TARGET_DIR="../../../../xcodebuild/Release"
HOST_SRC="$TARGET_DIR/remoting_me2me_host"
HOST_DST="./PrivilegedHelperTools/org.chromium.chromoting.me2me_host"
if [[ ! -f "$HOST_SRC" ]]; then
  error_exit "Unable to find $HOST_SRC";
fi
cp "$HOST_SRC" "$HOST_DST"

UNINSTALLER_SRC="$TARGET_DIR/remoting_host_uninstaller.app"
UNINSTALLER_DST="./Applications/Chrome Remote Desktop Host Uninstaller.app"
if [[ ! -d "$UNINSTALLER_SRC" ]]; then
  error_exit "Unable to find $UNINSTALLER_SRC";
fi
ditto "$UNINSTALLER_SRC" "$UNINSTALLER_DST"

# Verify that the host is the official build
OFFICIAL_CLIENTID=440925447803-avn2sj1kc099s0r7v62je5s339mu0am1
UNOFFICIAL_CLIENTID=440925447803-2pi3v45bff6tp1rde2f7q6lgbor3o5uj
grep -qF "$OFFICIAL_CLIENTID" "$HOST_DST"
if [[ "$?" != "0" ]]; then
  grep -qF "$UNOFFICIAL_CLIENTID" "$HOST_DST"
  if [[ "$?" == "0" ]]; then
    error_exit "Attempting to build with unoffical build";
  else
    error_exit "Unable to determine build type";
  fi
fi

# Unzip Keystone.
cd Keystone
unzip -qq -o GoogleSoftwareUpdate.pkg.zip
cd ..

# Build the .pkg.
echo "Building .pkg..."
freeze "$PKGPROJ_CHROMOTING"
freeze "$PKGPROJ_CRD_UNINSTALLER"
freeze "$PKGPROJ_CRD"

# Create the .dmg.
echo "Building .dmg..."
mkdir -p "$DMG_DIR/$PKG_CRD"
# Copy .mpkg installer.
ditto "$PKG_DIR/$PKG_CRD" "$DMG_DIR/$PKG_CRD"
# Copy .keystone_install script to top level of .dmg.
# Keystone calls this script during upgrades.
cp Scripts/keystone_install.sh "$DMG_DIR/.keystone_install"
# Build the .dmg from the directory.
hdiutil create "$DMG_FILENAME" -srcfolder "$DMG_DIR" -ov -quiet
rm -rf "$DMG_TEMP"
