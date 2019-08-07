#!/bin/bash

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# - Downloads all dependencies listed in package.json
# - Makes Chromium specific modifications.
# - Places the final output in components-chromium/

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"
check_dep "which rsync" "rsync" "apt-get install rsync"
check_dep "sed --version | grep GNU" \
    "GNU sed as 'sed'" "'brew install gnu-sed --with-default-names'"

pushd "$(dirname "$0")" > /dev/null

rm -rf node_modules

npm install --production

rsync -c --delete --delete-excluded -r -v --prune-empty-dirs \
    --exclude-from="rsync_exclude.txt" \
    "node_modules/@polymer/" \
    "node_modules/@webcomponents/" \
    "components-chromium/"

# Replace all occurrences of "@polymer/" with "../" or # "../../".
find components-chromium/ -mindepth 2 -maxdepth 2 -name '*.js' \
  -exec sed -i 's/@polymer\//..\//g' {} +
find components-chromium/ -mindepth 3 -maxdepth 3 -name '*.js' \
  -exec sed -i 's/@polymer\//..\/..\//g' {} +

# Replace all occurrences of "@webcomponents/" with "../".
#find . -name '*.js' -exec sed -i 's/@webcomponents\///g' {} +
find components-chromium/polymer/ -mindepth 3 -maxdepth 3 -name '*.js' \
  -exec sed -i 's/@webcomponents\//..\/..\/..\//g' {} +

# Apply additional chrome specific patches.
patch -p1 --forward -r - < chromium.patch

new=$(git status --porcelain components-chromium | grep '^??' | \
      cut -d' ' -f2 | egrep '\.(js|css)$' || true)

if [[ ! -z "${new}" ]]; then
  echo
  echo 'These files appear to have been added:'
  echo "${new}" | sed 's/^/  /'
fi

deleted=$(git status --porcelain components-chromium | grep '^.D' | \
          sed 's/^.//' | cut -d' ' -f2 | egrep '\.(js|css)$' || true)

if [[ ! -z "${deleted}" ]]; then
  echo
  echo 'These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

# TODO css_strip_prefixes.py
# TODO rgbify_hex_vars.py
# TODO create components summary
# TODO generate gn
# TODO find unused elements?
