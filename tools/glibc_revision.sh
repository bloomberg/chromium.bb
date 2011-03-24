#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

REVISION=0
cd "$(dirname "$0")"
# We need svn from depot_tools on Windows
FIXED_PATH="$(
  IFS=":"
  FIRST_ELEMENT=1
  for ELEMENT in $PATH ; do
    if [[ $ELEMENT = */depot_tools ]]; then
      if ((FIRST_ELEMENT)); then
        FIRST_ELEMENT=0
      else
        echo -n ":"
      fi
      echo -n "$ELEMENT"
    fi
  done
  for ELEMENT in $PATH ; do
    if [[ $ELEMENT != */depot_tools ]]; then
      if ((FIRST_ELEMENT)); then
        FIRST_ELEMENT=0
      else
        echo -n ":"
      fi
      echo -n "$ELEMENT"
    fi
  done
)"

for i in glibc_revision.sh REVISIONS Makefile ; do
  NREV="$(PATH="$FIXED_PATH" ; svn log "$(dirname "$0")/$i" 2>/dev/null |
                        head -n 2 | tail -n 1 | sed -e s'+r\([0-9]*\) .*+\1+')"
  if [[ "$NREV" -gt "$REVISION" ]]; then
    REVISION="$NREV"
  fi
done
echo "$REVISION"
