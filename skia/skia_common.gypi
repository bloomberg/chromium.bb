# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This gypi file handles the removal of platform-specific files from the
# Skia build.
{
  'conditions': [
    [ 'OS != "android"', {
      'sources/': [
         ['exclude', '_android\\.(cc|cpp)$'],
      ],
    }],
    [ 'OS != "ios"', {
      'sources/': [
         ['exclude', '_ios\\.(cc|cpp|mm?)$'],
      ],
    }],
    [ 'OS != "mac"', {
      'sources/': [
        ['exclude', '_mac\\.(cc|cpp|mm?)$'],
      ],
    }],
    [ 'OS != "win"', {
      'sources/': [ ['exclude', '_win\\.(cc|cpp)$'] ],
    }],
    [ 'use_glib == 0', {
      'sources/': [ ['exclude', '_linux\\.(cc|cpp)$'] ],
    }],
  ],

  'msvs_disabled_warnings': [4244, 4267, 4341, 4345, 4390, 4554, 4748, 4800],
}
