#!/bin/sh
# Script to build valgrind for use with chromium

THISDIR=`dirname $0`
THISDIR=`cd $THISDIR && /bin/pwd`
set -x
set -e

if ld --version | grep gold
then
  echo cannot build valgrind with gold
  exit 1
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

sh autogen.sh
./configure --prefix=/usr/local/valgrind-20090715
make -j4

if ./vg-in-place /bin/true
then
  echo built valgrind passes smoke test, good
else
  echo built valgrind fails smoke test
  exit 1
fi

sudo make install
cd /usr
test -f bin/valgrind && sudo mv bin/valgrind bin/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/bin/valgrind bin
test -d include/valgrind && sudo mv include/valgrind include/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/include/valgrind include
