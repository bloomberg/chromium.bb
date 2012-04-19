#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script builds the Chrome Remote Desktop installer and packages
# it into a .dmg.  It requires that Iceberg be installed (for 'freeze').

# The Chrome Remote Desktop installer consists of 2 components:
# * Chromoting installer package
# * Keystone (Google auto-update)

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
TARGET_DIR="../../../../out/Release"
HOST_SRC="$TARGET_DIR/remoting_me2me_host"
HOST_DST="PrivilegedHelperTools/org.chromium.chromoting.me2me_host"
cp "$HOST_SRC" "./$HOST_DST"
UNINSTALLER_SRC="$TARGET_DIR/remoting_host_uninstaller.app"
UNINSTALLER_DST="Applications/Chrome Remote Desktop Host Uninstaller.app"
ditto "$UNINSTALLER_SRC" "$UNINSTALLER_DST"

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
