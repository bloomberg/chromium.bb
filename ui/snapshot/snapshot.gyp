# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'snapshot',
      'type': '<(component)',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../../base/base.gyp:base',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../ui.gyp:ui',
      ],
      'defines': [
        'SNAPSHOT_IMPLEMENTATION',
      ],
      'sources': [
        'snapshot.h',
        'snapshot_android.cc',
        'snapshot_aura.cc',
        'snapshot_export.h',
        'snapshot_gtk.cc',
        'snapshot_ios.mm',
        'snapshot_mac.mm',
        'snapshot_win.cc',
        'snapshot_win.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../aura/aura.gyp:aura',
            '../compositor/compositor.gyp:compositor',
          ],
        }],
      ],
    },
    {
      'target_name': 'snapshot_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        '../events/events.gyp:events',
        '../gfx/gfx.gyp:gfx',
        '../ui.gyp:ui',
        'snapshot'
      ],
      'sources': [
        'snapshot_aura_unittest.cc',
        'snapshot_mac_unittest.mm',
        'test/run_all_unittests.cc',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '../../base/base.gyp:test_support_base',
            '../aura/aura.gyp:aura_test_support',
            '../compositor/compositor.gyp:compositor',
            '../compositor/compositor.gyp:compositor_test_support',
          ],
        }],
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and linux_use_tcmalloc==1', {
          'dependencies': [
            '../../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'snapshot_test_support',
          'type': 'static_library',
          'sources': [
            'test/snapshot_desktop.h',
            'test/snapshot_desktop_win.cc',
          ],
          'dependencies': [
            'snapshot',
          ],
          'include_dirs': [
            '../..',
          ],
        },
      ],
    }],
  ],
}
