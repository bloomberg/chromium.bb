#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
svn checkout --force \
    http://src.chromium.org/chrome/trunk/src/tools/deep_memory_profiler \
    deep_memory_profiler

curl -o deep_memory_profiler/pprof \
    http://src.chromium.org/chrome/trunk/src/third_party/tcmalloc/chromium/src/pprof

chmod a+x deep_memory_profiler/pprof
