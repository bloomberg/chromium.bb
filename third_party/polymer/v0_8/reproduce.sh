#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reproduces the content of 'components' and 'components-chromium' using the
# list of dependencies from 'bower.json'. Downloads needed packages and makes
# Chromium specific modifications. To launch the script you need 'bower',
# 'crisper', and 'vulcanize' installed on your system.

# IMPORTANT NOTE: The new vulcanize must be installed from
# https://github.com/Polymer/vulcanize/releases since it isn't on npm yet.

set -e

cd "$(dirname "$0")"

rm -rf components components-chromium

bower install

# These components are deprecated or needed only for demos.
rm -rf components/{iron-component-page,webcomponentsjs}

# Test and demo directories aren't needed.
rm -rf components/*/{test,demo}
rm -rf components/polymer/explainer

# Make checkperms.py happy.
find components/iron-selector -type f -exec chmod -x {} \;
chmod +x components/polymer/build.bat

# Remove carriage returns to make CQ happy.
find components -type f \( -name \*.html -o -name \*.css -o -name \*.js\
  -o -name \*.md -o -name \*.sh -o -name \*.json -o -name \*.gitignore\
  -o -name \*.bat \) -print0 | xargs -0 sed -i -e $'s/\r$//g'

./extract_inline_scripts.sh components components-chromium

# Actually fully vulcanize polymer.html to avoid needing to serve each file in
# the library separately.
vulcanize --inline-scripts components/polymer/polymer.html > components-chromium/polymer/polymer.html
crisper --source components-chromium/polymer/polymer.html\
  --html "components-chromium/polymer/polymer.html"\
  --js "components-chromium/polymer/polymer.js"
