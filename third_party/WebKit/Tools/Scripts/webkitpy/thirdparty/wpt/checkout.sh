#!/bin/bash
#
# Removes ./wpt/ directory containing the reduced web-platform-tests tree and
# starts a new checkout. Only files in WPTWhiteList are retained. The revisions
# getting checked out are defined in WPTHeads.

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd $DIR

TARGET_DIR=$DIR/wpt
REMOTE_REPO="https://chromium.googlesource.com/external/w3c/web-platform-tests.git"
WPT_HEAD=32aa301b33136f71b1e15594f07ea5345a6305db

function clone {
  # First line is the main repo HEAD.
  WPT_HEAD=$(head -n 1 $DIR/WPTHeads)

  # Remove existing repo if already exists.
  [ -d "$TARGET_DIR" ] && rm -rf $TARGET_DIR

  # Clone the main repository.
  git clone $REMOTE_REPO $TARGET_DIR
  cd $TARGET_DIR && git checkout $WPT_HEAD
  echo "WPTHead: " `git rev-parse HEAD`
}

function reduce {
  cd $TARGET_DIR
  # Some directories contain filenames with ' (single quote), and it confuses
  # xargs on some platforms, so we remove those directories first.
  rm -fr html css
  # Remove all except white-listed.
  find . -type f | grep -Fxvf ../WPTWhiteList | xargs -n 1 rm
  find . -empty -type d -delete
}

actions="clone reduce"
[ "$1" != "" ] && actions="$@"

for action in $actions; do
  type -t $action >/dev/null || (echo "Unknown action: $action" 1>&2 && exit 1)
  $action
done

chmod 755 $TARGET_DIR/tools/manifest/update.py

# TODO(burnik): Handle the SSL certs and other configuration.
