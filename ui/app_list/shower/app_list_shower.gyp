# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //ui/app_list/shower
      'target_name': 'app_list_shower',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        '../../aura/aura.gyp:aura',
        '../../compositor/compositor.gyp:compositor',
        '../../events/events.gyp:events_base',
        '../../events/events.gyp:events',
        '../../gfx/gfx.gyp:gfx_geometry',
        '../../views/views.gyp:views',
        '../app_list.gyp:app_list',
      ],
      'defines': [
        'APP_LIST_SHOWER_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'app_list_shower.h',
        'app_list_shower_delegate.h',
        'app_list_shower_delegate_factory.h',
        'app_list_shower_export.h',
        'app_list_shower_impl.cc',
        'app_list_shower_impl.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # GN version: //ui/app_list/shower:test_support
      'target_name': 'app_list_shower_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../skia/skia.gyp:skia',
        'app_list_shower',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'test/app_list_shower_impl_test_api.cc',
        'test/app_list_shower_impl_test_api.h',
      ],
    },
    {
      # GN version: //ui/app_list/shower:app_list_shower_unittests
      'target_name': 'app_list_shower_unittests',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../base/base.gyp:test_support_base',
        '../../../skia/skia.gyp:skia',
        '../../../testing/gtest.gyp:gtest',
        '../../aura/aura.gyp:aura_test_support',
        '../../resources/ui_resources.gyp:ui_test_pak',
        '../../views/views.gyp:views',
        '../../wm/wm.gyp:wm',
        '../app_list.gyp:app_list_test_support',
        'app_list_shower',
        'app_list_shower_test_support',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'app_list_shower_impl_unittest.cc',
        'test/run_all_unittests.cc',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'app_list_shower_unittests_run',
          'type': 'none',
          'dependencies': [
            'app_list_shower_unittests',
          ],
          'includes': [
            '../../../build/isolate.gypi',
          ],
          'sources': [
            'app_list_shower_unittests.isolate',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../../../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
