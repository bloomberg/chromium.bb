# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains flags that are specific to o3d, but affect the build as a
# whole (e.g. build flags that change the ABI).
# For example, on linux amd64, all the libraries that will be linked into the
# plugin (skia, v8, ...) need to be compiled with -fPIC but we don't want to do
# that generally in Chrome (so it can't be lumped into the top-level
# build/common.gypi).
{
  'variables': {
    'conditions': [
      [ 'OS == "linux"', {
        'use_system_zlib': 0,
      }],
    ],
  },
  'target_defaults': {
    'conditions': [
      [ 'OS == "linux" and target_arch=="x64"', {
        'cflags': [
          '-m64',
          '-fPIC',
        ],
        'ldflags': [
          '-m64',
        ],
      }],
      [ 'OS == "mac"', {
          'xcode_settings': {
            'MACOSX_DEPLOYMENT_TARGET': '10.5',
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'OTHER_CFLAGS': ['-mmacosx-version-min=10.5'],
          },
          'defines': [
            'MAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_5',
          ],
      }],
    ],
  },
}
