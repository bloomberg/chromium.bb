#!/bin/bash
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# Clones the dom-distiller repo, compiles and extracts its javascript Then
# copies that js into the Chromium tree.
# This script should be run from the src/ directory and requires that ant is
# installed. It takes an optional parameter for which SHA1 to roll to. If left
# unspecified the script rolls to HEAD.

(
  set -e

  dom_distiller_js_path=third_party/dom_distiller_js
  dom_distiller_js_package=$dom_distiller_js_path/package
  readme_chromium=$dom_distiller_js_path/README.chromium
  tmpdir=/tmp/domdistiller-$$
  changes=$tmpdir/domdistiller.changes
  bugs=$tmpdir/domdistiller.bugs
  curr_gitsha=$(grep 'Version:' $readme_chromium | awk '{print $2}')

  rm -rf $tmpdir
  mkdir $tmpdir

  pushd $tmpdir
  git clone https://github.com/chromium/dom-distiller/ .

  # The new git SHA1 is HEAD or the first command line parameter.
  [[ -z "$1" ]] && gitsha_target="HEAD" || gitsha_target="$1"
  new_gitsha=$(git rev-parse --short=10 ${gitsha_target})
  git reset --hard ${new_gitsha}
  git log --oneline ${curr_gitsha}..${new_gitsha} > $changes

  echo -n BUG= > $bugs

  # This extracts BUG= lines from the log, extracts the numbers part, removes
  # whitespace and deletes empty lines. Then, split on ',', sort, uniquify and
  # rejoin. Finally, remove the trailing ',' and concat to $bugs.
  git log ${curr_gitsha}..${new_gitsha} \
    | grep BUG= \
    | sed -e 's/.*BUG=\(.*\)/\1/' -e 's/\s*//g' -e '/^$/d' \
    | tr ',' '\n' \
    | sort \
    | uniq \
    | tr '\n' ',' \
    | head --bytes=-1 \
    >> $bugs

  echo >> $bugs  # add a newline

  ant package
  popd

  rm -rf $dom_distiller_js_package
  mkdir $dom_distiller_js_package
  cp -rf $tmpdir/out/package/* $dom_distiller_js_package
  git add $dom_distiller_js_package
  cp $tmpdir/LICENSE $dom_distiller_js_path/
  sed -i "s/Version: [0-9a-f]*/Version: $new_gitsha/" $readme_chromium

  gen_message () {
    echo "Roll DomDistillerJS"
    echo
    echo "Picked up changes:"
    cat $changes
    echo
    cat $bugs
  }

  message=$tmpdir/message
  gen_message > $message

  # Run checklicenses.py on the pulled files, but only print the output on
  # failures.
  tools/checklicenses/checklicenses.py $dom_distiller_js_path > $tmpdir/checklicenses.out || cat $tmpdir/checklicenses.out

  git commit -a -F $message

  rm -rf $tmpdir
)
