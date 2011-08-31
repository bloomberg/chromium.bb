#!/bin/bash
# A script to send toolchain edits in tools/SRC (in git) to toolchain trybots.

# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "$(dirname "$0")"
. REVISIONS

repos='binutils gcc gdb glibc linux-headers-for-nacl newlib'
trybots="\
nacl-toolchain-lucid64-glibc,\
nacl-toolchain-mac-glibc,\
nacl-toolchain-win7-glibc\
"

tmp='.git-status$$'
trap 'rm -f $tmp' 0 1 2 15
{ git status --porcelain -uno > "$tmp" &&
  [ -r "$tmp" ] && [ ! -s "$tmp" ]
} || {
  echo >&2 "$0: Start with a clean working directory"
  exit 1
}

tryname=try
for repo in $repos; do
  revname="NACL_$(echo "$repo" | tr '[:lower:]-' '[:upper:]_')_COMMIT"
  patch="toolchain-try.${repo}.patch"
  (cd "SRC/$repo"; git diff "${!revname}..HEAD") > $patch
  if [ $? -ne 0 ]; then
    echo >&2 "$0: error: update SRC/$repo first"
    exit 2
  fi
  if [ -s "$patch" ]; then
    git add "$patch"
    tryname="${tryname}-${repo}-$(cd "SRC/$repo";
                                  git rev-list -n1 --abbrev-commit HEAD)"
  else
    rm -f "$patch"
  fi
done

if ! git rev-parse origin/master >/dev/null; then
  echo >&2 "$0: error: no origin/master branch"
  exit 3
fi

(set -x
 git checkout -b "$tryname" origin/master
 git commit -m "toolchain trybot run: $tryname"

 git try -b "$trybots"
)
