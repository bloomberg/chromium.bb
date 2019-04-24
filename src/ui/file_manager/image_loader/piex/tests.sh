#!/bin/bash

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PORT=8123
HTTP="http://localhost:${PORT}"

# Start test http server.
echo -e "test: starting test server ${HTTP}\n" > tests.log
node node_modules/http-server/bin/http-server -p ${PORT} > /dev/null 2>&1 &
HTTP_SERVER_PID=$!

# Extract preview thumbnails from the raw test images.
rm -f tests.hash
node tests.js ${HTTP}/tests.html "$*" | tee -a tests.log | \
  grep hash > tests.hash
kill ${HTTP_SERVER_PID} > /dev/null 2>&1

# Compare their hash to the golden hash values.
if [[ $(cmp tests.hash images.golden.hash) ]]; then
  echo "tests FAIL" || exit 1
else
  echo "tests PASS"
fi
