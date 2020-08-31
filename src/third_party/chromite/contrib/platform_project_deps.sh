#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A helper script to dump the library dependencies of a platform2 project.
# Accepts a single package name as an argument.

# You must run `setup_board` for the target in $BOARD before running this script.

BOARD="amd64-generic"

[[ -z "$1" ]] && echo "You need to supply a package name." && exit 1

ROUGH_PKG_ATOM="$1"
PKG_MATCHES=()
readarray -t PKG_MATCHES < \
    <("equery-${BOARD}" -qCN list -o --format=\$category/\$name "${ROUGH_PKG_ATOM}")

if [[ ${#PKG_MATCHES[@]} -eq 0 ]];
then
    echo "No packages could be found matching \"${ROUGH_PKG_ATOM}\""
    exit 1
else
    PKG="${PKG_MATCHES[0]}"
    echo "Matched package: ${PKG}"
fi


EBUILD_PATH="$("equery-${BOARD}" which "${PKG}")"

INCREMENTAL_BUILD=$(sed -n -r -e 's/^(CROS_WORKON_INCREMENTAL_BUILD="?)([01])"?$/\2/p' "${EBUILD_PATH}")
if [[ ${INCREMENTAL_BUILD} -eq 1 ]]; then
    readonly BUILD_DIR="/build/${BOARD}/var/cache/portage/${PKG}/out/Default"
else
    readonly BUILD_DIR="/build/${BOARD}/tmp/portage/${PKG}-9999/work/build/out/Default"
fi

PLATFORM_SUBDIR=$(sed -n -r -e 's/^(PLATFORM_SUBDIR=")([^"]+)"$/\2/p' "${EBUILD_PATH}")

# Emerge the dependencies of $PKG but not $PKG itself `-o`, including build-time dependencies,
# using binary prebuilts `-g`, in parallel `-j`, quietly `-q`.
"emerge-${BOARD}" -jogq --with-bdeps=y "${PKG}"

# Unpack the source of PKG and run the ebuild through src_configure, where `gn gen`
# is called.
"ebuild-${BOARD}" "${EBUILD_PATH}" clean configure

pushd "/mnt/host/source/src/platform2/${PLATFORM_SUBDIR}" &>/dev/null || exit 1

GN_TARGETS=()
for t in executable shared_library
do
    mapfile -t < <(gn ls "${BUILD_DIR}" --type="${t}")
    GN_TARGETS+=( "${MAPFILE[@]}" )
done

LIB_DEPS=()
for gt in "${GN_TARGETS[@]}"
do
    mapfile -t < <(gn desc "${BUILD_DIR}" "${gt}" libs --all)
    LIB_DEPS+=( "${MAPFILE[@]}" )
done
popd &>/dev/null || exit 1

echo ""

echo "The package ${PKG} consumes the following libraries:"
printf '%s\n' "${LIB_DEPS[@]}" | sort -u
