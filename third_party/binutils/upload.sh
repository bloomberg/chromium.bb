#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Upload the generated output to Google storage.

if [ ! -d $1 ]; then
  echo "update.sh <output directory from build-all.sh>"
  exit 1
fi

if [ ! -f ~/.boto ]; then
  echo "You need to run 'gsutil config' to set up authentication before running this script."
  exit 1
fi

BINUTILS_TAR_BZ2=linux/binutils.tar.bz2
if [ -f ${BINUTILS_TAR_BZ2}.sha1 ]; then
  echo "Please remove ${BINUTILS_TAR_BZ2}.sha1 before starting..."
  exit 1
fi

(cd $1/; tar -jcvf ../$BINUTILS_TAR_BZ2 .)
../depot_tools/upload_to_google_storage.py --bucket chromium-binutils $BINUTILS_TAR_BZ2
