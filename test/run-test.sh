#!/bin/sh
FONTDIR=`pwd`/fonts
CACHEFILE=`pwd`/fonts.cache

ECHO=true

FCLIST=../fc-list/fc-list
FCCACHE=../fc-cache/fc-cache

check () {
  $FCLIST - family pixelsize | sort > out
  echo "=" >> out
  $FCLIST - family pixelsize | sort >> out
  echo "=" >> out
  $FCLIST - family pixelsize | sort >> out
  tr -d '\015' <out >out.tmp; mv out.tmp out
  if cmp out out.expected > /dev/null ; then : ; else
    echo "*** Test failed: $TEST"
    echo "*** output is in 'out', expected output in 'out.expected'"
    exit
  fi
}

prep() {
  rm -rf $CACHEFILE
  rm -rf $FONTDIR
  mkdir $FONTDIR
}

dotest () {
  TEST=$1
  test x$VERBOSE = x || echo Running: $TEST
}

sed "s!@FONTDIR@!$FONTDIR!
s!@CACHEFILE@!$CACHEFILE!" < fonts.conf.in > fonts.conf

FONTCONFIG_FILE=`pwd`/fonts.conf
export FONTCONFIG_FILE

dotest "Basic check"
prep
cp 4x6.pcf 8x16.pcf $FONTDIR
check

dotest "With a subdir"
prep
cp 4x6.pcf 8x16.pcf $FONTDIR
$FCCACHE $FONTDIR
check

dotest "Subdir with a cache file"
prep
mkdir $FONTDIR/a
cp 4x6.pcf 8x16.pcf $FONTDIR/a
$FCCACHE $FONTDIR/a
check

dotest "Complicated directory structure"
prep
mkdir $FONTDIR/a
mkdir $FONTDIR/a/a
mkdir $FONTDIR/b
mkdir $FONTDIR/b/a
cp 4x6.pcf $FONTDIR/a
cp 8x16.pcf $FONTDIR/b/a
check

dotest "Subdir with an out-of-date cache file"
prep
mkdir $FONTDIR/a
$FCCACHE $FONTDIR/a
sleep 1
cp 4x6.pcf 8x16.pcf $FONTDIR/a
check

dotest "Dir with an out-of-date cache file"
prep
cp 4x6.pcf $FONTDIR
$FCCACHE $FONTDIR
sleep 1
mkdir $FONTDIR/a
cp 8x16.pcf $FONTDIR/a
check

rm -rf $FONTDIR $CACHEFILE $FONTCONFIG_FILE out
