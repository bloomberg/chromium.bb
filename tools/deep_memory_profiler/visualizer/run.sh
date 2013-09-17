#!/bin/bash
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This shell would copy flot library from chromium/src/third_parth.
mkdir static/third_party/flot
cp ../../../third_party/flot/jquery.min.js static/third_party/flot
cp ../../../third_party/flot/jquery.flot.min.js static/third_party/flot
cp ../../../third_party/flot/jquery.flot.stack.min.js static/third_party/flot
