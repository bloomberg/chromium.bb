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

# These components are needed only for demos and docs.
rm -rf components/{hydrolysis,marked,marked-element,prism,prism-element,\
iron-component-page,iron-doc-viewer,webcomponentsjs}

# Remove unused gzipped binary which causes git-cl problems.
rm components/web-animations-js/web-animations.min.js.gz

# Test and demo directories aren't needed.
rm -rf components/*/{test,demo}
rm -rf components/polymer/explainer

# Make checkperms.py happy.
find components/*/hero.svg -type f -exec chmod -x {} \;
find components/iron-selector -type f -exec chmod -x {} \;

# Remove carriage returns to make CQ happy.
find components -type f \( -name \*.html -o -name \*.css -o -name \*.js\
  -o -name \*.md -o -name \*.sh -o -name \*.json -o -name \*.gitignore\
  -o -name \*.bat \) -print0 | xargs -0 sed -i -e $'s/\r$//g'

# Resolve a unicode encoding issue in dom-innerHTML.html.
NBSP=$(python -c 'print u"\u00A0".encode("utf-8")')
sed -i 's/['"$NBSP"']/\\u00A0/g' components/polymer/polymer-mini.html

./extract_inline_scripts.sh components components-chromium
