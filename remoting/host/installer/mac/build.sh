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
PKGPROJ_CHROME_REMOTE_DESKTOP='Chrome Remote Desktop.packproj'
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
HOST_BINARY=PrivilegedHelperTools/org.chromium.chromoting.me2me_host
cp ../../../../out/Release/remoting_me2me_host ./$HOST_BINARY

# Build the .pkg.
echo "Building .pkg..."
freeze "$PKGPROJ_CHROMOTING"
freeze "$PKGPROJ_CHROME_REMOTE_DESKTOP"

# Create the .dmg.
echo "Building .dmg..."
mkdir -p "$DMG_DIR/$PKG_CRD"
# Copy .mpkg installer.
ditto "$PKG_DIR/$PKG_CRD" "$DMG_DIR/$PKG_CRD"
# Copy uninstall script.
# TODO(garykac): This should be made into a proper App and installed in the
# Applications directory.
cp Scripts/uninstall.sh "$DMG_DIR"
# Copy .keystone_install script to top level of .dmg.
# Keystone calls this script during upgrades.
cp Scripts/keystone_install.sh "$DMG_DIR/.keystone_install"
# Build the .dmg from the directory.
hdiutil create "$DMG_FILENAME" -srcfolder "$DMG_DIR" -ov -quiet
rm -rf "$DMG_TEMP"
