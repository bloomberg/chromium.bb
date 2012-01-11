#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

tmp=$(mktemp -t chromium-update-copyrights.XXXXX)
trap "rm -f $tmp" EXIT
git diff --name-only $(git cl upstream)... | while read file; do
    cp "$file" "$tmp"
    # Rather than editing the temporary file, edit the original file in-place
    # so that we preserve file modes.
    sed -i -e '1,4s/Copyright .c. .* The Chromium Authors/Copyright (c) 2012 The Chromium Authors/' "$file"
    if ! diff -q "$file" "$tmp" > /dev/null; then
        echo "updated $file"
    fi
done
