#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reproduces the content of 'components' and 'components-chromium' using the
# list of dependencies from 'bower.json'. Downloads needed packages and makes
# Chromium specific modifications. To launch the script you need 'bower' and
# 'vulcanize' to be install in your system.

set -e

cd "$(dirname "$0")"

rm -rf components components-chromium
rm -rf ../web-animations-js/sources

bower install

rm -rf components/web-animations-js/{test,node_modules}
# TODO(jlklein): Remove when
# https://github.com/web-animations/web-animations-next/pull/289 is released
# and the version of web-animations-js is bumped in bower.json.
rm components/web-animations-js/.travis-setup.sh

mv components/web-animations-js ../web-animations-js/sources
cp ../web-animations-js/sources/COPYING ../web-animations-js/LICENSE

# These components are deprecated or needed only for demos.
rm -rf components/{core-component-page,core-field,font-roboto,webcomponentsjs}

# Make checkperms.py happy.
find components/{core-list,core-resizable} -type f -exec chmod -x {} \;

# Remove carriage returns to make CQ happy.
find components -type f \( -name \*.html -o -name \*.css -o -name \*.js\
  -o -name \*.md -o -name \*.sh -o -name \*.json -o -name \*.gitignore \)\
  -print0 | xargs -0 sed -i -e $'s/\r$//g'

# TODO(dzhioev): remove when
# https://code.google.com/p/grit-i18n/issues/detail?id=30 is fixed
patch -p1 < chromium.patch

./extract_inline_scripts.sh components components-chromium

