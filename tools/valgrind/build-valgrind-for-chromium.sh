#!/bin/sh
set -x
set -e

# Check out latest version that following patches known to apply against
rm -rf valgrind-20090715
svn co -r '{2009-07-15}' svn://svn.valgrind.org/valgrind/trunk valgrind-20090715

cd valgrind-20090715

# Just in case, make sure svn gets the right version of the external VEX repo, too
cd VEX
svn update -r '{2009-07-15}'
cd ..

# Work around bug https://bugs.kde.org/show_bug.cgi?id=162848
# fork() not handled properly
wget "https://bugs.kde.org/attachment.cgi?id=35150"
patch -p0 < "attachment.cgi?id=35150"

# Work around bug https://bugs.kde.org/show_bug.cgi?id=186796
# long suppressions truncated
wget "https://bugs.kde.org/attachment.cgi?id=35174"
patch -p0 < "attachment.cgi?id=35174"

sh autogen.sh
./configure --prefix=/usr/local/valgrind-20090715
make
sudo make install
cd /usr
test -f bin/valgrind && sudo mv bin/valgrind bin/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/bin/valgrind bin
test -d include/valgrind && sudo mv include/valgrind include/valgrind.orig
sudo ln -sf /usr/local/valgrind-20090715/include/valgrind include
