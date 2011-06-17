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


# Replace each "/nacl64/bin/bar" with a link to "/bin/nacl64-bar"
for exe in "$prefix/nacl64/bin/"*; do
  name="$(basename "$exe")"
  ln -sfn "../../bin/nacl64-$name" "$exe"
done


# For each "/bin/nacl64-foo" create "/bin/nacl-foo" redirector
for exe in "$prefix/bin/nacl64-"*; do
  name="$(basename "$exe")"
  ./create_redirector.sh -s "$prefix/bin/nacl-${name/nacl64-}" ||
      ln -sfn "$name" "$prefix/bin/nacl-${name/nacl64-}"
done


# For each "/nacl64/bin/bar" create "/nacl/bin/bar" redirector
for exe in "$prefix/nacl64/bin/"*; do
  name="$(basename "$exe")"
  ./create_redirector.sh -t -s "$prefix/nacl/bin/$name" ||
      ln -sfn "../../bin/nacl64-$name" "$prefix/nacl/bin/$name"
done
