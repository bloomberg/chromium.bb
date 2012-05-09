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

ME="$(basename "${0}")"
readonly ME

declare -a g_cleanup_dirs

# Binaries to sign.
ME2ME_HOST='PrivilegedHelperTools/org.chromium.chromoting.me2me_host'
UNINSTALLER='Applications/@@HOST_UNINSTALLER_NAME@@.app'

# The Chromoting Host installer is a meta-package that consists of 3
# components:
#  * Chromoting Host Service package
#  * Chromoting Host Uninstaller package
#  * Keystone package(GoogleSoftwareUpdate - for Official builds only)
PKGPROJ_HOST='ChromotingHost.packproj'
PKGPROJ_HOST_SERVICE='ChromotingHostService.packproj'
PKGPROJ_HOST_UNINSTALLER='ChromotingHostUninstaller.packproj'

# Final (user-visible) mpkg name.
PKG_FINAL='@@HOST_PKG@@.mpkg'

DMG_VOLUME_NAME='@@DMG_VOLUME_NAME@@'
DMG_FILE_NAME='@@DMG_FILE_NAME@@.dmg'

# Temp directory for Iceberg output.
PKG_DIR=build
g_cleanup_dirs+=("${PKG_DIR}")

# Temp directories for building the dmg.
DMG_TEMP_DIR="$(mktemp -d -t "${ME}"-dmg)"
g_cleanup_dirs+=("${DMG_TEMP_DIR}")

DMG_EMPTY_DIR="$(mktemp -d -t "${ME}"-empty)"
g_cleanup_dirs+=("${DMG_EMPTY_DIR}")

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

verify_clean_dir() {
  local dir="${1}"
  if [[ ! -d "${dir}" ]]; then
    mkdir "${dir}"
  fi

  if [[ -e "${output_dir}/${DMG_FILE_NAME}" ]]; then
    err "Output directory is dirty from previous build."
    exit 1
  fi
}

sign() {
  local name="${1}"
  local keychain="${2}"
  local id="${3}"

  if [[ ! -e "${name}" ]]; then
    err_exit "Input file doesn't exist: ${name}"
  fi

  echo Signing "${name}"
  codesign -vv -s "${id}" --keychain "${keychain}" "${name}"
  codesign -v "${name}"
}

sign_binaries() {
  local input_dir="${1}"
  local keychain="${2}"
  local id="${3}"

  sign "${input_dir}/${ME2ME_HOST}" "${keychain}" "${id}"
  sign "${input_dir}/${UNINSTALLER}" "${keychain}" "${id}"
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

  # Create the .dmg.
  echo "Building .dmg..."
  "${input_dir}/pkg-dmg" \
      --format UDBZ \
      --tempdir "${DMG_TEMP_DIR}" \
      --source "${DMG_EMPTY_DIR}" \
      --target "${output_dir}/${DMG_FILE_NAME}" \
      --volname "${DMG_VOLUME_NAME}" \
      --copy "${input_dir}/${PKG_DIR}/${PKG_FINAL}" \
      --copy "${input_dir}/Scripts/keystone_install.sh:/.keystone_install"

  if [[ ! -f "${output_dir}/${DMG_FILE_NAME}" ]]; then
    err_exit "Unable to create disk image: ${DMG_FILE_NAME}"
  fi
}

cleanup() {
  if [[ "${#g_cleanup_dirs[@]}" > 0 ]]; then
    rm -rf "${g_cleanup_dirs[@]}"
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

  verify_clean_dir "${output_dir}"

  sign_binaries "${input_dir}" "${codesign_keychain}" "${codesign_id}"
  build_packages "${input_dir}"
  # TODO(garykac): Sign final .mpkg.
  build_dmg "${input_dir}" "${output_dir}"

  cleanup
}

if [[ ${#} -ne 4 ]]; then
  usage
  exit 1
fi

main "${@}"
exit ${?}
