#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ME2ME_HOST="/Library/PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app"
UNINSTALLER="/Applications/Chrome Remote Desktop Host Uninstaller.app"
PREFPANE="/Library/PreferencePanes/org.chromium.chromoting.prefPane"
KEYSTONE="/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle"

INFO_PLIST="Contents/Info.plist"

set -e -u

function print_plist_version {
  local name="${1}"
  local file="${2}"
  set `PlistBuddy -c 'Print CFBundleVersion' "${file}/${INFO_PLIST}"`
  echo "${name} version = ${1}"
}

print_plist_version "Me2me host" "${ME2ME_HOST}"
print_plist_version "Uninstaller" "${UNINSTALLER}"
print_plist_version "PreferencePane" "${PREFPANE}"
print_plist_version "Keystone" "${KEYSTONE}"
