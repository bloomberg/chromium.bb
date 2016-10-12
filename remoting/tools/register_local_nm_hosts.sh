#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Script that can be used to register native messaging hosts in the output
# directory.

set -e

SRC_DIR="$(readlink -f "$(dirname "$0")/../..")"
ME2ME_HOST_NAME="com.google.chrome.remote_desktop"
IT2ME_HOST_NAME="com.google.chrome.remote_assistance"

install_manifest() {
  local manifest_template="$1"
  local host_path="$2"
  local host_path_var_name="$3"
  local target_dir="$4"

  local template_name="$(basename ${manifest_template})"
  local manifest_name="${template_name%.*}"
  local target_manifest="${target_dir}/${manifest_name}"

  echo Registering ${host_path} in ${target_manifest}
  mkdir -p "${target_dir}"
  sed -e "s#{{ ${host_path_var_name} }}#${host_path}#g" \
    < "$manifest_template" > "$target_manifest"
}

register_hosts() {
  local build_dir="$1"
  local chrome_data_dir="$2"

  install_manifest \
     "${SRC_DIR}/remoting/host/setup/${ME2ME_HOST_NAME}.json.jinja2" \
     "${build_dir}/remoting_native_messaging_host" \
     ME2ME_HOST_PATH "${chrome_data_dir}"

  install_manifest \
     "${SRC_DIR}/remoting/host/it2me/${IT2ME_HOST_NAME}.json.jinja2" \
     "${build_dir}/remote_assistance_host" \
     IT2ME_HOST_PATH "${chrome_data_dir}"
}

register_hosts_for_all_channels() {
  local build_dir="$1"

  if [ -n "$CHROME_USER_DATA_DIR" ]; then
    register_hosts "${build_dir}" \
        "${CHROME_USER_DATA_DIR}/NativeMessagingHosts"
  elif [ $(uname -s) == "Darwin" ]; then
    register_hosts "${build_dir}" \
        "${HOME}/Library/Application Support/Google/Chrome/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/Library/Application Support/Chromium/NativeMessagingHosts"
  else
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome-beta/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/google-chrome-unstable/NativeMessagingHosts"
    register_hosts "${build_dir}" \
        "${HOME}/.config/chromium/NativeMessagingHosts"
  fi
}

unregister_hosts() {
  local chrome_data_dir="$1"

  rm -f "${chrome_data_dir}/${ME2ME_HOST_NAME}.json"
  rm -f "${chrome_data_dir}/${IT2ME_HOST_NAME}.json"
}

unregister_hosts_for_all_channels() {
  if [ -n "$CHROME_USER_DATA_DIR" ]; then
    unregister_hosts \
        "${CHROME_USER_DATA_DIR}/NativeMessagingHosts"
  elif [ $(uname -s) == "Darwin" ]; then
    unregister_hosts \
        "${HOME}/Library/Application Support/Google/Chrome/NativeMessagingHosts"
    unregister_hosts \
        "${HOME}/Library/Application Support/Chromium/NativeMessagingHosts"
  else
    unregister_hosts "${HOME}/.config/google-chrome/NativeMessagingHosts"
    unregister_hosts "${HOME}/.config/google-chrome-beta/NativeMessagingHosts"
    unregister_hosts \
        "${HOME}/.config/google-chrome-unstable/NativeMessagingHosts"
    unregister_hosts "${HOME}/.config/chromium/NativeMessagingHosts"
  fi
}

print_usage() {
  echo "Usage: $0 [-r|-u]" >&2
  echo "   -r   Register Release build instead of Debug" >&2
  echo "   -u   Unregister" >&2
}

build_dir="Debug"

if [[ $# -gt 1 ]]; then
  print_usage
elif [[ $# -eq 1 ]]; then
  case "$1" in
    "-r")
      build_dir="Release"
      ;;
    "-u")
      unregister_hosts_for_all_channels
      exit 0
      ;;
    *)
      print_usage
      exit 1
      ;;
  esac
fi

register_hosts_for_all_channels "${SRC_DIR}/out/${build_dir}"
