#!/bin/bash
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does some preparations before build of instrumented pulseaudio.

# Disable problematic assembly in clang builds.
patch -p1 < $(dirname ${BASH_SOURCE[0]})/pulseaudio.diff

# The configure script enforces FORTIFY_SOURCE=2, but we can't live with that.
sed -i "s/-D_FORTIFY_SOURCE=2/-U_FORTIFY_SOURCE/g" ./configure
