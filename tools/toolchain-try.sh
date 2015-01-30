#!/bin/bash
# A script to send toolchain edits in tools/SRC (in git) to toolchain trybots.

# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "$(dirname "$0")"
source REVISIONS
if [[ ! -e "../.svn/gcl_info/changes/toolchain-try" ]] ; then
  # Create randomly named CL that consumes all local SVN edits.
  random_name=""
  while ((${#random_name}<16)); do
    random_name="$random_name$(
      dd if=/dev/urandom bs=1 count=1 |
      LC_ALL=C grep '[A-Za-z]')"
  done
  SVN_EDITOR=true gcl change "$random_name"
fi

# Lazily create a CL called 'toolchain-try' to contain patches to code in
# tools/SRC.
for i in binutils gcc glibc linux-headers-for-nacl newlib; do (
  revname="NACL_$(echo $i | tr '[:lower:]' '[:upper:]')_COMMIT"
  if [[ "$revname" = "NACL_LINUX-HEADERS-FOR-NACL_COMMIT" ]] ; then
    revname="LINUX_HEADERS_FOR_NACL_COMMIT"
  fi
  cd "SRC/$i"
  git diff "${!revname}..HEAD" > ../../"toolchain-try.$i.patch" )
  svn add "toolchain-try.$i.patch"
done
if [[ ! -e "../.svn/gcl_info/changes/toolchain-try" ]] ; then
  SVN_EDITOR=true gcl change toolchain-try
  rm "../.svn/gcl_info/changes/$random_name"
fi
BOTS=nacl-toolchain-precise64-glibc,nacl-toolchain-mac-glibc,\
nacl-toolchain-win7-glibc
if (( $# >= 1 )) ; then
  gcl try toolchain-try --bot $BOTS -n "$1"
else
  gcl try toolchain-try --bot $BOTS
fi
