#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reproduces the content of 'components' and 'components-chromium' using the
# list of dependencies from 'bower.json'. Downloads needed packages and makes
# Chromium specific modifications. To launch the script you need 'bower' and
# 'crisper' installed on your system.

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"
check_dep "which bower" "bower" "npm install -g bower"
check_dep "which crisper" "crisper" "npm install -g crisper"
check_dep "which uglifyjs" "uglifyjs" "npm install -g uglifyjs"
check_dep "which polymer-css-build" "polymer-css-build" \
    "npm install -g polymer-css-build"
check_dep "which rsync" "rsync" "apt-get install rsync"
check_dep "python -c 'import bs4'" "bs4" "apt-get install python-bs4"

set -e

pushd "$(dirname "$0")" > /dev/null

rm -rf components
rm -rf ../../web-animations-js/sources

bower install --no-color

rm components/*/.travis.yml

mv components/web-animations-js ../../web-animations-js/sources
cp ../../web-animations-js/sources/COPYING ../../web-animations-js/LICENSE

# Remove source mapping directives since we don't compile the maps.
sed -i 's/^\s*\/\/#\s*sourceMappingURL.*//' \
  ../../web-animations-js/sources/*.min.js

# These components are needed only for demos and docs.
rm -rf components/{hydrolysis,marked,marked-element,prism,prism-element,\
iron-component-page,iron-doc-viewer,webcomponentsjs}

# Test and demo directories aren't needed.
rm -rf components/*/{test,demo}
rm -rf components/polymer/explainer

# Remove promise-polyfill and components which depend on it.
rm -rf components/promise-polyfill
rm -rf components/iron-ajax
rm -rf components/iron-form

# Make checkperms.py happy.
find components/*/hero.svg -type f -exec chmod -x {} \;
find components/iron-selector -type f -exec chmod -x {} \;

# Remove carriage returns to make CQ happy.
find components -type f \( -name \*.html -o -name \*.css -o -name \*.js\
  -o -name \*.md -o -name \*.sh -o -name \*.json -o -name \*.gitignore\
  -o -name \*.bat -o -name \*.svg \) -print0 | xargs -0 sed -i -e $'s/\r$//g'

# Resolve a unicode encoding issue in dom-innerHTML.html.
NBSP=$(python -c 'print u"\u00A0".encode("utf-8")')
sed -i 's/['"$NBSP"']/\\u00A0/g' components/polymer/polymer-mini.html

./extract_inline_scripts.sh components components-chromium

# Remove import of external resource in font-roboto (fonts.googleapis.com)
# and apply additional chrome specific patches. NOTE: Where possible create
# a Polymer issue and/or pull request to minimize these patches.
patch -p1 --forward -r - < chromium.patch

new=$(git status --porcelain components-chromium | grep '^??' | \
      cut -d' ' -f2 | egrep '\.(html|js|css)$' || true)

if [[ ! -z "${new}" ]]; then
  echo
  echo 'These files appear to have been added:'
  echo "${new}" | sed 's/^/  /'
fi

deleted=$(git status --porcelain components-chromium | grep '^.D' | \
          sed 's/^.//' | cut -d' ' -f2 | egrep '\.(html|js|css)$' || true)

if [[ ! -z "${deleted}" ]]; then
  echo
  echo 'These files appear to have been removed:'
  echo "${deleted}" | sed 's/^/  /'
fi

if [[ ! -z "${new}${deleted}" ]]; then
  echo
fi

echo 'Creating a summary of components...'
python create_components_summary.py > components_summary.txt

echo 'Creating GYP files for interfaces and externs...'
./generate_gyp.sh

echo 'Vulcanizing dependent UIs...'
python ../../../chrome/browser/resources/vulcanize.py

popd > /dev/null

echo 'Searching for unused elements...'
python "$(dirname "$0")"/find_unused_elements.py
