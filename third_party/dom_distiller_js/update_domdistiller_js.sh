#!/bin/bash
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# Clones the dom-distiller repo, compiles and extracts its javascript Then
# copies that js into the Chromium tree.
# This script should be run from the src/ directory and requires that ant is
# installed.

(
  dom_distiller_js_path=third_party/dom_distiller_js
  compiled_js_path=$dom_distiller_js_path/js/domdistiller.js
  readme_chromium=$dom_distiller_js_path/README.chromium
  tmpdir=/tmp/domdistiller-$$
  changes=/tmp/domdistiller.changes
  curr_gitsha=$(grep 'Version:' $readme_chromium | awk '{print $2}')

  rm -rf $tmpdir
  mkdir $tmpdir

  pushd $tmpdir
  git clone https://code.google.com/p/dom-distiller/ .
  ant extractjs
  new_gitsha=$(git rev-parse --short=10 HEAD)
  git log --oneline ${curr_gitsha}.. > $changes
  popd

  mkdir -p $(dirname $compiled_js_path)
  cp $tmpdir/out/domdistiller.js $compiled_js_path
  cp $tmpdir/LICENSE $dom_distiller_js_path/
  sed -i "s/Version: [0-9a-f]*/Version: $new_gitsha/" $readme_chromium

  echo "Picked up changes:"
  cat $changes

  rm -rf $tmpdir
  rm $changes
)
