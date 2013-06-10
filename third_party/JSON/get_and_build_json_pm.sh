#!/bin/bash
# Download and build JSON.pm
# Homepage:
# http://search.cpan.org/~makamaka/JSON-2.58/lib/JSON.pm
# SRC_URL='http://www.cpan.org/authors/id/M/MA/MAKAMAKA/JSON-2.58.tar.gz'
PACKAGE='JSON'
VERSION='2.59'
SRC_URL="http://www.cpan.org/authors/id/M/MA/MAKAMAKA/$PACKAGE-$VERSION.tar.gz"
FILENAME="$(basename $SRC_URL)"
SHA1_FILENAME="$FILENAME.sha1"
BUILD_DIR="$PACKAGE-$VERSION"
INSTALL_DIR="$(pwd)/out"

curl --remote-name "$SRC_URL"

# Check hash
# SHA-1 hash generated via:
# shasum JSON-2.59.tar.gz > JSON-2.59.tar.gz.sha1
if ! [ -f "$SHA1_FILENAME" ]
then
  echo "SHA-1 hash file $SHA1_FILENAME not found, could not verify archive"
  exit 1
fi

# Check that hash file contains hash for archive
HASHFILE_REGEX="^[0-9a-f]{40}  $FILENAME"  # 40-digit hash, followed by filename
if ! grep --extended-regex --line-regex --silent \
  "$HASHFILE_REGEX" "$SHA1_FILENAME"
then
  echo "SHA-1 hash file $SHA1_FILENAME does not contain hash for $FILENAME," \
       'could not verify archive'
  echo 'Hash file contents are:'
  cat "$SHA1_FILENAME"
  exit 1
fi

if ! shasum --check "$SHA1_FILENAME"
then
  echo 'SHA-1 hash does not match,' \
       "archive file $FILENAME corrupt or compromised!"
  exit 1
fi

# Extract and build
tar xvzf "$FILENAME"
cd "$BUILD_DIR"
perl Makefile.PL INSTALL_BASE="$INSTALL_DIR"
make
make test
make install
cd ..
rm "$FILENAME"

# Rename :: to __ because : is reserved in Windows filenames
# (only occurs in man pages, which aren't necessary)
for i in $(find . -name '*::*')
do
  mv -f "$i" `echo "$i" | sed s/::/__/g`
done

# Fix permissions and shebangs
# https://rt.cpan.org/Public/Bug/Display.html?id=85917
# Make examples executable
cd "$BUILD_DIR"
chmod +x eg/*.pl
cd t

# Strip shebangs from test files that have them
for i in *.t
do
  if head -1 "$i" | grep --quiet '^#!'
  then
    ed -s "$i" <<END
# Delete line 1
1d
# Write and Quit
wq
END
  fi
done
