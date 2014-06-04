#!/bin/bash -e

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install build dependencies of packages which we instrument. 

grep \'\<\(\_sanitizer_type \
     $(dirname ${BASH_SOURCE[0]})/instrumented_libraries.gyp | \
     sed "s;\s*'<(_sanitizer_type)-\(.*\)',;\1;" | sort | uniq | \
     xargs sudo apt-get build-dep -y

# Extra build deps for pulseaudio, which apt-get build-dep may fail to install
# for reasons which are not entirely clear. 
sudo apt-get install libltdl3-dev libjson0-dev \
         libsndfile1-dev libspeexdsp-dev \
         chrpath -y  # Chrpath is required by fix_rpaths.sh.
