# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'features.gypi',
  ],
  'targets': [
    {
      # This target creates config.h suitable for a WebKit-V8 build and
      # copies a few other files around as needed.
      'target_name': 'config',
      'type': 'none',
      'msvs_guid': '2E2D3301-2EC4-4C0F-B889-87073B30F673',
      'direct_dependent_settings': {
        'defines': [
          '<@(feature_defines)',
          '<@(non_feature_defines)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'], 
          'direct_dependent_settings': {
            'defines': [
              '__STD_C',
              '_CRT_SECURE_NO_DEPRECATE', 
              '_SCL_SECURE_NO_DEPRECATE',
            ],
            'include_dirs': [
              '../third_party/WebKit/JavaScriptCore/os-win32',
              'build/JavaScriptCore',
            ],
          },
        }],
      ],
    },
  ], # targets
}
