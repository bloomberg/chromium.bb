#!/bin/bash

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PORT=9915

echo "http://$(hostname):${PORT}/view.html"
exec python -m SimpleHTTPServer $PORT
