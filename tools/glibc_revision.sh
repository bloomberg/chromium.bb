#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

REVISION=0
cd "$(dirname "$0")"
if [[ "$(uname -s)" = Darwin ]] || [[ "$(uname -o)" != "Cygwin" ]] ; then
  SVN="svn"
else
  SVN="svn.bat"
fi
for i in REVISIONS glibc_revision.sh Makefile ; do
  NREV="$("$SVN" log "$i" 2>/dev/null | head -n 2 | tail -n 1 |
                                                sed -e s'+r\([0-9]*\) .*+\1+')"
  if [[ "$NREV" -gt "$REVISION" ]]; then
    REVISION="$NREV"
  fi
done
echo "$REVISION"
if [[ "$REVISION" == "0" ]]; then
  exit 1
else
  exit 0
fi
