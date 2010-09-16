#!/bin/bash
#
# Copyright 2007 Google Inc. All Rights Reserved.
# Author: shess@google.com (Scott Hess)

cd src

mkdir bld
cd bld
../configure
FILES="keywordhash.h opcodes.c opcodes.h parse.c parse.h sqlite3.h"
OPTS=""
# These options should match those in ../../tools/config.mk.
OPTS="$OPTS -DSQLITE_OMIT_LOAD_EXTENSION=1"
make "OPTS=$OPTS" $FILES
cp -f $FILES ../../preprocessed

cd ..
rm -rf bld

# TODO(shess) I can't find config.h, which exists in the original
# third_party/sqlite/ directory.  I also haven't found a client of it,
# yet, so maybe it's not a file we need.
