#!/bin/sh

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script signs the Chromoting binaries, builds the Chrome Remote Desktop
# installer and then packages it into a .dmg.  It requires that Packages be
# installed (for 'packagesbuild').
# Packages: http://s.sudre.free.fr/Software/Packages/about.html
#
# usage: sign_and_build.sh output_dir input_dir codesign_keychain codesign_id
#
# The final disk image (dmg) is placed in |output_dir|.

set -e -u

ME="$(basename "${0}")"
readonly ME

declare -a g_cleanup_dirs

setup() {
  local input_dir="${1}"

  # The file that contains the properties for this signing build.
  # The file should contain only key=value pairs, one per line.
  PROPS_FILENAME="${input_dir}/do_signing.props"

  # Individually load the properties for this build. Don't 'source' the file
  # to guard against code accidentally being added to the props file.
  HOST_UNINSTALLER_NAME=$(read_property "HOST_UNINSTALLER_NAME")
  HOST_PKG=$(read_property "HOST_PKG")
  DMG_VOLUME_NAME=$(read_property "DMG_VOLUME_NAME")
  DMG_FILE_NAME=$(read_property "DMG_FILE_NAME")

  # Binaries to sign.
  ME2ME_HOST='PrivilegedHelperTools/org.chromium.chromoting.me2me_host.app'
  UNINSTALLER="Applications/${HOST_UNINSTALLER_NAME}.app"
  PREFPANE='PreferencePanes/org.chromium.chromoting.prefPane'

  # The Chromoting Host installer is a meta-package that consists of 3
  # components:
  #  * Chromoting Host Service package
  #  * Chromoting Host Uninstaller package
  #  * Keystone package (GoogleSoftwareUpdate - for Official builds only)
  PKGPROJ_HOST='ChromotingHost.pkgproj'
  PKGPROJ_HOST_SERVICE='ChromotingHostService.pkgproj'
  PKGPROJ_HOST_UNINSTALLER='ChromotingHostUninstaller.pkgproj'

  # Final (user-visible) pkg name.
  PKG_FINAL="${HOST_PKG}.pkg"

  DMG_FILE_NAME="${DMG_FILE_NAME}.dmg"

  # Temp directory for Packages output.
  PKG_DIR=build
  g_cleanup_dirs+=("${PKG_DIR}")

  # Temp directories for building the dmg.
  DMG_TEMP_DIR="$(mktemp -d -t "${ME}"-dmg)"
  g_cleanup_dirs+=("${DMG_TEMP_DIR}")

  DMG_EMPTY_DIR="$(mktemp -d -t "${ME}"-empty)"
  g_cleanup_dirs+=("${DMG_EMPTY_DIR}")
}

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

# Read a single property from the properties file.
read_property() {
  local property="${1}"
  local filename="${PROPS_FILENAME}"
  echo `grep "${property}" "${filename}" | tail -n 1 | cut -d "=" -f2-`
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
  sign "${input_dir}/${PREFPANE}" "${keychain}" "${id}"
}

sign_installer() {
  local input_dir="${1}"
  local keychain="${2}"
  local id="${3}"

  local package="${input_dir}/${PKG_DIR}/${PKG_FINAL}"
  productsign --sign "${id}" --keychain "${keychain}" \
      "${package}" "${package}.signed"
  mv -f "${package}.signed" "${package}"
}

build_package() {
  local pkg="${1}"
  echo "Building .pkg from ${pkg}"
  packagesbuild -v "${pkg}"
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
  echo "Usage: ${ME}: output_dir input_dir keychain codesign_id"\
      "[productsign_id]" >&2
}

main() {
  local output_dir="$(shell_safe_path "${1}")"
  local input_dir="$(shell_safe_path "${2}")"
  local keychain="$(shell_safe_path "${3}")"
  local codesign_id="${4}"
  local productsign_id=""
  if [[ ${#} -ge 5 ]]; then
    productsign_id="${5}"
  fi

  setup "${input_dir}"
  verify_clean_dir "${output_dir}"

  sign_binaries "${input_dir}" "${keychain}" "${codesign_id}"
  build_packages "${input_dir}"
  if [[ -n "${productsign_id}" ]]; then
    echo "Signing installer..."
    sign_installer "${input_dir}" "${keychain}" "${productsign_id}"
  fi
  build_dmg "${input_dir}" "${output_dir}"

  cleanup
}

if [[ ${#} < 4 ]]; then
  usage
  exit 1
fi

main "${@}"
exit ${?}
