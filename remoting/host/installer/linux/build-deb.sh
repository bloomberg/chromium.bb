#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPTDIR="$(dirname "$(readlink -f "$0")")"
PACKAGE_NAME="chrome-remote-desktop"

guess_filename() {
  ARCH=$(dpkg-architecture | awk -F '=' '/DEB_BUILD_ARCH=/{print $2}')
  VERSION_FULL=$(get_version_full)
  echo ${PACKAGE_NAME}_${VERSION_FULL}_${ARCH}.deb
}

get_version_full() {
  src_root=${src_root:-./../../../..}
  remoting_version_path=$src_root/remoting/VERSION
  chrome_version_path=$src_root/chrome/VERSION
  version_helper=$src_root/chrome/tools/build/version.py

  # TODO(lambroslambrou): Refactor to share the logic with remoting.gyp.
  version_major=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@MAJOR@")
  version_minor=$($version_helper -f $remoting_version_path \
                  -t "@REMOTING_PATCH@")
  version_build=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@BUILD@")
  version_patch=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@PATCH@")
  version_full="$version_major.$version_minor.$version_build.$version_patch"
  echo $version_full
}

usage() {
  echo "usage: $(basename $0) [-hp] [-o path] [-s path]"
  echo "-h     this help message"
  echo "-p     just print the expected DEB filename that this will build."
  echo "-s     path to the top of the src tree."
  echo "-o     path to write the DEB file to."
}

while getopts ":s:o:ph" OPTNAME
do
  case $OPTNAME in
    s )
      src_root="$(readlink -f "$OPTARG")"
      ;;
    o )
      OUTPUT_PATH="$(readlink -f "$OPTARG")"
      ;;
    p )
      PRINTDEBNAME=1
      ;;
    h )
      usage
      exit 0
      ;;
    \: )
      echo "'-$OPTARG' needs an argument."
      usage
      exit 1
      ;;
    * )
      echo "invalid command-line option: $OPTARG"
      usage
      exit 1
      ;;
  esac
done
shift $(($OPTIND - 1))

# This just prints the expected package filename, then exits. It's needed so the
# gyp packaging target can track the output file, to know whether or not it
# needs to be built/rebuilt.
if [[ -n "$PRINTDEBNAME" ]]; then
  guess_filename
  exit 0
fi

# TODO: Make this all happen in a temp dir to keep intermediate files out of the
# build tree?
cd "$SCRIPTDIR"

if [[ -z "$version_full" ]]; then
  version_full=$(get_version_full)
fi

if [[ ! "$version_full" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Error: Invalid \$version_full value: $version_full" >&2
  exit 1
fi

# Include revision information in changelog when building from a local
# git-based checkout.
merge_head="$(git merge-base HEAD origin/git-svn 2>/dev/null || true)"
if [[ -n "$merge_head" ]]; then
  revision="$(git svn find-rev "$merge_head" 2>/dev/null || true)"
else
  # Official builders still use svn-based builds.
  revision="$(svn info . | awk '/^Revision: /{print $2}')"
fi
if [[ -n "$revision" ]]; then
  revision_text="(r$revision)"
fi

echo "Building version $version_full $revision_text"

# Create a fresh debian/changelog.
export DEBEMAIL="The Chromium Authors <chromium-dev@chromium.org>"
rm -f debian/changelog
debchange --create \
  --package "$PACKAGE_NAME" \
  --newversion "$version_full" \
  --force-distribution \
  --distribution unstable \
  "New Debian package $revision_text"

# TODO(mmoss): This is a workaround for a problem where dpkg-shlibdeps was
# resolving deps using some of our build output shlibs (i.e.
# out/Release/lib.target/libfreetype.so.6), and was then failing with:
#   dpkg-shlibdeps: error: no dependency information found for ...
# It's not clear if we ever want to look in LD_LIBRARY_PATH to resolve deps,
# but it seems that we don't currently, so this is the most expediant fix.
SAVE_LDLP=$LD_LIBRARY_PATH
unset LD_LIBRARY_PATH
dpkg-buildpackage -b -us -uc
LD_LIBRARY_PATH=$SAVE_LDLP

if [[ "$OUTPUT_PATH" ]]; then
  mv ../${PACKAGE_NAME}_*.deb "$OUTPUT_PATH"/
  mv ../${PACKAGE_NAME}_*.changes "$OUTPUT_PATH"/
fi
