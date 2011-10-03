#!/bin/bash

# Libhnj is dual licensed under LGPL and MPL. Boilerplate for both
# licenses follows.

# LibHnj - a library for high quality hyphenation and justification
# Copyright (C) 1998 Raph Levien,
# 	     (C) 2001 ALTLinux, Moscow (http://www.alt-linux.org),
#           (C) 2001 Peter Novodvorsky (nidd@cs.msu.su)
#           (C) 2006, 2007, 2008, 2010 László Németh (nemeth at OOo)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA  02111-1307  USA.

# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
#
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.

function check_valgrind_log () {
if [ "$VALGRIND" != "" ]; then
  if [ -f $TEMPDIR/test.pid* ]; then
    log=`ls $TEMPDIR/test.pid*`
    if ! grep -q 'ERROR SUMMARY: 0 error' $log; then
        echo "Fail in $NAME $1 checking detected by Valgrind"
        echo "$log Valgrind log file moved to $TEMPDIR/badlogs"
        mv $log $TEMPDIR/badlogs
        exit 1
    fi
    if grep -q 'LEAK SUMMARY' $log; then
        echo "Memory leak in $NAME $1 checking detected by Valgrind"
        echo "$log Valgrind log file moved to $TEMPDIR/badlogs"
        mv $log $TEMPDIR/badlogs
        exit 1
    fi    
    rm -f $log
  fi
fi
}

if [ -d ../../_build/../tests ]; then
  TESTDIR="../../tests"
else
  TESTDIR="."
fi

TEMPDIR="./testSubDir"
NAME="$1"

if [ ! -d $TEMPDIR ]; then
  mkdir $TEMPDIR
fi

shopt -s expand_aliases

alias example='../libtool --mode=execute -dlopen ../.libs/libhyphen*.la ../example'

if [ "$VALGRIND" != "" ]; then
  rm -f $TEMPDIR/test.pid*
  if [ ! -d $TEMPDIR/badlogs ]; then
    mkdir $TEMPDIR/badlogs
  fi
  if [ ! -f ../.libs/lt-example ]; then
    echo "Use make check before Valgrind tests"
  else
  alias example='../libtool --mode=execute -dlopen ../.libs/libhyphen*.la valgrind --tool=$VALGRIND --leak-check=yes --show-reachable=yes --log-file=$TEMPDIR/test.pid ../example'
  fi
fi

example $TESTDIR/$1 $TESTDIR/$2 >$TEMPDIR/test.out$$
diff $TEMPDIR/test.out$$ $TESTDIR/$3 || exit 1
#diff $TEMPDIR/test.out$$ $TESTDIR/$3 && rm -f $TEMPDIR/*$$ || exit 1

check_valgrind_log "VALGRIND LOG"
