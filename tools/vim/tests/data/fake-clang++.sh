#!/bin/sh

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

echo 'Fake clang'
echo '#include <...> search starts here:'
echo ' {chrome_root}/include/a'
echo ' {chrome_root}/include/b'
echo 'End of search list.'
echo 'Some other random junk.'
