#!/bin/sh
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "$(dirname "$0")/../source/data/coll"

echo "Removing unihan collation data from CJK"

for i in zh ja ko
do
  echo "Overwriting ${i}.txt"
  sed '/^        unihan{$/,/^        }$/ d' -i ${i}.txt
done
