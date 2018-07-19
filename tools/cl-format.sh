#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

for f in $(git diff --name-only @{u}); do
  if echo $f | sed -n '/\.\(cc\|h\)$/q1;q0'; then
    continue;
  fi
  clang-format -style=file -i "$f"
done
