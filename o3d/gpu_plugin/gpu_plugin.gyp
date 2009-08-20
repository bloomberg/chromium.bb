# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'gpu_plugin',
      'type': '<(library)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'gpu_plugin.cc',
        'gpu_plugin.h',
      ],
    },
    
    # This is a standalone executable until O3D is fully moved over to using
    # gyp. At that point these can become part of the regular O3D unit tests.
    {
      'target_name': 'gpu_plugin_unittests',
      'type': 'executable',
      'dependencies': [
        'gpu_plugin',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gmock.gyp:gmockmain',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '../..',
        '../../third_party/npapi',
      ],
      'sources': [
        'gpu_plugin_unittest.cc',
      ],
    },
  ]
}
