#!/bin/sh
# Script to build valgrind for use with chromium

THISDIR=`dirname $0`
THISDIR=`cd $THISDIR && /bin/pwd`
set -x
set -e

if ld --version | grep gold
then
  # build/install-build-deps leaves original ld around, try using that
  if test -x /usr/bin/ld.orig
  then
    echo "Using /usr/bin/ld.orig instead of gold to link valgrind"
    test -d $THISDIR/override_ld && rm -rf $THISDIR/override_ld
    mkdir $THISDIR/override_ld
    ln -s /usr/bin/ld.orig $THISDIR/override_ld/ld
    PATH="$THISDIR/override_ld:$PATH"
  else
    echo "Cannot build valgrind with gold.  Please switch to normal /usr/bin/ld, rerun this script, then switch back to gold."
    exit 1
  fi
fi

# Check out latest version that following patches known to apply against
rm -rf valgrind-20090715
svn co -r '{2009-07-15}' svn://svn.valgrind.org/valgrind/trunk valgrind-20090715

cd valgrind-20090715

# Just in case, make sure svn gets the right version of the external VEX repo, too
cd VEX
svn update -r '{2009-07-15}'
cd ..

# Work around bug https://bugs.kde.org/show_bug.cgi?id=162848
# "fork() not handled properly"
#wget -O fork.patch "https://bugs.kde.org/attachment.cgi?id=35510"
patch -p0 < "$THISDIR"/fork.patch

# Work around bug https://bugs.kde.org/show_bug.cgi?id=186796
# "long suppressions truncated"
#wget -O longlines.patch "https://bugs.kde.org/attachment.cgi?id=35174"
patch -p0 < "$THISDIR"/longlines.patch

# Work around bug http://bugs.kde.org/186790
# "Suppression counts do not include leak suppressions"
patch -p0 < "$THISDIR"/leak.patch

# Add feature bug https://bugs.kde.org/show_bug.cgi?id=201170
# "Want --show-possible option so I can ignore the bazillion possible leaks..."
#wget -O possible.patch https://bugs.kde.org/attachment.cgi?id=35559
patch -p0 < "$THISDIR"/possible.patch

if [ "$INSTALL_TSAN" = "yes" ]
then
  # Add ThreadSanitier to the installation.
  # ThreadSanitizer is an experimental dynamic data race detector.
  # See http://code.google.com/p/data-race-test/wiki/ThreadSanitizer
  svn checkout -r 1096 http://data-race-test.googlecode.com/svn/trunk/tsan tsan
  mkdir tsan/{docs,tests}
  touch tsan/{docs,tests}/Makefile.am
  patch -p 0 < tsan/valgrind.patch
  patch -p 0 -d VEX < tsan/vex.patch
fi

sh autogen.sh
./configure --prefix=/usr/local/valgrind-20090715
make -j4

if ./vg-in-place true
then
  echo built valgrind passes smoke test, good
else
  echo built valgrind fails smoke test
  exit 1
fi

test -d $THISDIR/override_ld && rm -rf $THISDIR/override_ld

sudo make install
cd /usr
test -f bin/valgrind && sudo mv bin/valgrind bin/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/bin/valgrind bin
test -d include/valgrind && sudo mv include/valgrind include/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/include/valgrind include
