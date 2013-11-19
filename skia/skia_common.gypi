# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This gypi file handles the removal of platform-specific files from the
# Skia build.
{
  'include_dirs': [
    '..',
    'config',
  ],

  'conditions': [
    [ 'OS != "android"', {
      'sources/': [
         ['exclude', '_android\\.(cc|cpp)$'],
      ],
    }],
    [ 'OS == "android"', {
      'defines': [
        'SK_FONTHOST_DOES_NOT_USE_FONTMGR',
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
    [ 'desktop_linux == 0 and chromeos == 0', {
      'sources/': [ ['exclude', '_linux\\.(cc|cpp)$'] ],
    }],
    [ 'use_cairo == 0', {
      'sources/': [ ['exclude', '_cairo\\.(cc|cpp)$'] ],
    }],
  ],

  # We would prefer this to be direct_dependent_settings,
  # however we currently have no means to enforce that direct dependents
  # re-export if they include Skia headers in their public headers.
  'all_dependent_settings': {
    'include_dirs': [
      '..',
      'config',
    ],
  },

  'msvs_disabled_warnings': [4244, 4267, 4341, 4345, 4390, 4554, 4748, 4800],
}
