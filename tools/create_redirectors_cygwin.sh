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

if ! cygcheck -V >/dev/null 2>/dev/null; then
  echo "No cygcheck found"
  exit 1
fi

echo "WARNING: hang can occur on FAT, use NTFS"


# Before:
#   bin/
#     nacl64-foo
#     nacl64-bar
#   nacl64/
#     bin/
#       bar
#
# After(=> redirector):
#   bin/
#     nacl64-foo => ../libexec/nacl64-foo
#     nacl64-bar => ../libexec/nacl64-bar
#     nacl-foo   => ../libexec/nacl64-foo
#     nacl-bar   => ../libexec/nacl64-bar
#   libexec/
#     nacl64-foo
#     nacl64-bar
#   nacl64/
#     bin/
#       bar => ../../libexec/nacl64-bar
#
# Symbolic links vs. hard links:
#   On windows/cygwin, hard links are needed to run linked programs outside of
#   the cygwin shell. On *nix, there is no usage difference.
#   Here we handle only the windows/cygwin case and use the hard links.


# Move each "/bin/nacl64-foo" to "/libexec/nacl64-foo" unless it is a redirector
for exe in "$prefix/bin/nacl64-"*.exe; do
  if ! cmp -s ./redirector.exe "$exe"; then
    mv -f "$exe" "$prefix/libexec/"
  fi
done


# For each "/libexec/nacl64-foo" create "/bin/nacl64-foo" and "/bin/nacl-foo"
for exe in "$prefix/libexec/nacl64-"*.exe; do
  name="$(basename "$exe")"
  ln -fn ./redirector.exe "$prefix/bin/$name"
  ln -fn ./redirector.exe "$prefix/bin/nacl-${name/nacl64-}"
done


# For each "/nacl64/bin/bar" create "/nacl64/bin/bar"
for exe in "$prefix/nacl64/bin/"*.exe; do
  name="$(basename "$exe")"
  ln -fn ./redirector.exe "$prefix/nacl64/bin/$name"
done


# Inject DLLs:
#   get list of (directory, dll):
#     for each exe:
#       run cygcheck to get list of DLLs
#     keep unique pairs only
#   for each (directory, dll):
#     if dll is from /usr/bin:
#       add link to the dll in the directory
#
# We dump all DLLs and filter /usr/bin DLLs later to save cygpath calls.


find "$prefix" -name "*.exe" -print0 | while read -r -d $'\0' exe; do
  dir="$(dirname "$exe")"
  win_exe="$(cygpath -w "$exe")"
  cd "$dir" && cygcheck "$win_exe" | while read -r win_dll; do
    echo "$dir:$win_dll"
  done
done | sort -u | while IFS=':' read -r dir win_dll; do
  dll="$(cygpath "$win_dll")"
  if [[ "$dll" = /usr/bin/*.dll ]]; then
    ln -fn "$dll" "$dir"
  fi
done
