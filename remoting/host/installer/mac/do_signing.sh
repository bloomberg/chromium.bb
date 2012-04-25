#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script signs the Chromoting binaries, builds the Chrome Remote Desktop
# installer and then packages it into a .dmg.  It requires that Iceberg be
# installed (for 'freeze').
#
# usage: sign_and_build.sh output_dir input_dir codesign_keychain codesign_id
#
# The final disk image (dmg) is placed in |output_dir|.

set -e -u

# Binaries to sign.
ME2ME_HOST=PrivilegedHelperTools/org.chromium.chromoting.me2me_host

# Iceberg creates this directory to write its output.
PKG_DIR=build

# The Chromoting Host installer is a meta-package that consists of 3
# components:
#  * Chromoting Host Service package
#  * Chromoting Host Uninstaller package
#  * Keystone package(GoogleSoftwareUpdate - for Official builds only)
PKGPROJ_HOST='ChromotingHost.packproj'
PKGPROJ_HOST_SERVICE='ChromotingHostService.packproj'
PKGPROJ_HOST_UNINSTALLER='ChromotingHostUninstaller.packproj'

# Final mpkg name (for Official builds).
PKG_FINAL='ChromeRemoteDesktopHost.mpkg'

DMG_TEMP=dmg_tmp
DMG_NAME='Chrome Remote Desktop'
DMG_DIR="${DMG_TEMP}/${DMG_NAME}"
DMG_FILENAME='Chrome Remote Desktop.dmg'

ME="$(basename "${0}")"
readonly ME

err() {
  echo "[$(date +'%Y-%m-%d %H:%M:%S%z')]: ${@}" >&2
}

err_exit() {
  err "${@}"
  exit 1
}

# shell_safe_path ensures that |path| is safe to pass to tools as a
# command-line argument. If the first character in |path| is "-", "./" is
# prepended to it. The possibly-modified |path| is output.
shell_safe_path() {
  local path="${1}"
  if [[ "${path:0:1}" = "-" ]]; then
    echo "./${path}"
  else
    echo "${path}"
  fi
}

verify_empty_dir() {
  local dir="${1}"
  if [[ ! -d "${dir}" ]]; then
    mkdir "${dir}"
  fi

  shopt -s nullglob dotglob
  local dir_contents=("${dir}"/*)
  shopt -u nullglob dotglob

  if [[ ${#dir_contents[@]} -ne 0 ]]; then
    err "output directory must be empty"
    exit 1
  fi
}

sign_binaries() {
  local input_dir="${1}"
  local keychain="${2}"
  local id="${3}"

  me2me_host="${input_dir}/${ME2ME_HOST}"
  if [[ ! -f "${me2me_host}" ]]; then
    err_exit "Input file doesn't exist: ${me2me_host}"
  fi

  echo Signing "${me2me_host}"
  codesign -vv -s "${id}" --keychain "${keychain}" "${me2me_host}"

  # Verify signing.
  codesign -v "${me2me_host}"
}

build_package() {
  local pkg="${1}"
  echo "Building .pkg from ${pkg}"
  freeze "${pkg}"
}

build_packages() {
  local input_dir="${1}"
  build_package "${input_dir}/${PKGPROJ_HOST_SERVICE}"
  build_package "${input_dir}/${PKGPROJ_HOST_UNINSTALLER}"
  build_package "${input_dir}/${PKGPROJ_HOST}"
}

build_dmg() {
  local input_dir="${1}"
  local output_dir="${2}"

  # TODO(garykac): Change this to use the pkg-dmg script.

  # Create the .dmg.
  echo "Building .dmg..."
  mkdir -p "${input_dir}/${DMG_DIR}/${PKG_FINAL}"
  # Copy .mpkg installer.
  ditto "${input_dir}/${PKG_DIR}/${PKG_FINAL}" \
      "${input_dir}/${DMG_DIR}/${PKG_FINAL}"
  # Copy .keystone_install script to top level of .dmg.
  # Keystone calls this script during upgrades.
  cp "${input_dir}/Scripts/keystone_install.sh" \
      "${input_dir}/${DMG_DIR}/.keystone_install"
  # Build the .dmg from the directory.
  hdiutil create "${output_dir}/${DMG_FILENAME}" \
      -srcfolder "${input_dir}/${DMG_DIR}" -ov -quiet

  if [[ ! -f "${output_dir}/${DMG_FILENAME}" ]]; then
    err_exit "Unable to create disk image: ${DMG_FILENAME}"
  fi
}

usage() {
  echo "Usage: ${ME}: output_dir input_dir codesign_keychain codesign_id" >&2
}

main() {
  local output_dir="$(shell_safe_path "${1}")"
  local input_dir="$(shell_safe_path "${2}")"
  local codesign_keychain="$(shell_safe_path "${3}")"
  local codesign_id="${4}"

  verify_empty_dir "${output_dir}"

  sign_binaries "${input_dir}" "${codesign_keychain}" "${codesign_id}"
  build_packages "${input_dir}"
  # TODO(garykac): Sign final .mpkg.
  build_dmg "${input_dir}" "${output_dir}"
}

if [[ ${#} -ne 4 ]]; then
  usage
  exit 1
fi

main "${@}"
exit ${?}
