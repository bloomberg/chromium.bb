#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -Ceu


prefix="$1"

if [[ ! -d "$prefix" ]]; then
  echo "Usage: $0 toolchain-prefix"
  exit 1
fi


# Before:
#   bin/
#     nacl64-foo
#     nacl64-bar
#   nacl64/
#     bin/
#       bar
#
# After(-> link, => redirector or link):
#   bin/
#     nacl64-foo
#     nacl64-bar
#     nacl-foo => nacl64-foo
#     nacl-bar => nacl64-bar
#   nacl64/
#     bin/
#       bar -> ../../nacl64-bar
#   nacl/
#     bin/
#       bar => ../../nacl64-bar
#
# Symbolic links vs. hard links:
#   On windows/cygwin, hard links are needed to run linked programs outside of
#   the cygwin shell. On *nix, there is no usage difference.
#   Here we handle only the *nix case and use the symbolic links.

while read from to action ; do
  if [[ -e "$prefix/`dirname $from`/$to" ]] ; then
    ./create_redirector.sh "$prefix/$from" "$to" "$action"
  fi
done < redirect_table.txt
