#!/bin/bash

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
