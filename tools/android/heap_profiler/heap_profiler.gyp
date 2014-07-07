# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # libheap_profiler is the library that will be preloaded in the Android
      # Zygote and contains the black magic to hook malloc/mmap calls.
      'target_name': 'heap_profiler',
      'type': 'shared_library',
      'include_dirs': [ '../../..' ],
      'sources': [ 'heap_profiler_hooks_android.c' ],
      'dependencies': [ 'heap_profiler_core' ],
    },
    {
      # heap_profiler_core contains only the tracking metadata code without any
      # hooks. It is required by both the hprof library itself and the unittest.
      'target_name': 'heap_profiler_core',
      'type': 'static_library',
      'sources': [
        'heap_profiler.c',
        'heap_profiler.h',
      ],
      'include_dirs': [ '../../..' ],
    },
    {
      'target_name': 'heap_dump',
      'type': 'executable',
      'sources': [ 'heap_dump.c' ],
      'include_dirs': [ '../../..' ],
    },
    {
      'target_name': 'heap_profiler_unittests',
      'type': 'executable',
      'sources': [ 'heap_profiler_unittest.cc' ],
      'dependencies': [
        'heap_profiler_core',
        '../../../testing/gtest.gyp:gtest',
        '../../../testing/gtest.gyp:gtest_main',
      ],
      'include_dirs': [ '../../..' ],
    },
    {
      'target_name': 'heap_profiler_unittests_stripped',
      'type': 'none',
      'dependencies': [ 'heap_profiler_unittests' ],
      'actions': [{
        'action_name': 'strip heap_profiler_unittests',
        'inputs': [ '<(PRODUCT_DIR)/heap_profiler_unittests' ],
        'outputs': [ '<(PRODUCT_DIR)/heap_profiler_unittests_stripped' ],
        'action': [ '<(android_strip)', '<@(_inputs)', '-o', '<@(_outputs)' ],
      }],
    },
    {
      'target_name': 'heap_profiler_integrationtest',
      'type': 'executable',
      'sources': [ 'heap_profiler_integrationtest.cc' ],
      'dependencies': [ '../../../testing/gtest.gyp:gtest' ],
      'include_dirs': [ '../../..' ],
    },
    {
      'target_name': 'heap_profiler_integrationtest_stripped',
      'type': 'none',
      'dependencies': [ 'heap_profiler_integrationtest' ],
      'actions': [{
        'action_name': 'strip heap_profiler_integrationtest',
        'inputs': [ '<(PRODUCT_DIR)/heap_profiler_integrationtest' ],
        'outputs': [ '<(PRODUCT_DIR)/heap_profiler_integrationtest_stripped' ],
        'action': [ '<(android_strip)', '<@(_inputs)', '-o', '<@(_outputs)' ],
      }],
    },
  ],
}
