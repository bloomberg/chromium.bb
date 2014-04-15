# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'external_ozone_platforms': [],
    'external_ozone_platform_files': [],
    'external_ozone_platform_deps': [],
  },
  'targets': [
    {
      'target_name': 'ozone',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ui/events/events.gyp:events',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<@(external_ozone_platform_deps)',
      ],
      'defines': [
        'OZONE_IMPLEMENTATION',
      ],
      'variables': {
        'platform_list_file': '<(SHARED_INTERMEDIATE_DIR)/ui/ozone/ozone_platform_list.cc',
        'ozone_platforms': [
          '<@(external_ozone_platforms)',
        ],
      },
      'sources': [
        '<(platform_list_file)',
        # common/chromeos files are excluded automatically when building with
        # chromeos=0, by exclusion rules in filename_rules.gypi due to the
        # 'chromeos' folder name.
        'common/chromeos/native_display_delegate_ozone.cc',
        'common/chromeos/native_display_delegate_ozone.h',
        'ozone_platform.cc',
        'ozone_platform.h',
        'ozone_switches.cc',
        'ozone_switches.h',
        'platform/dri/ozone_platform_dri.cc',
        'platform/dri/ozone_platform_dri.h',
        'platform/dri/cursor_factory_evdev_dri.cc',
        'platform/dri/cursor_factory_evdev_dri.h',
        'platform/test/ozone_platform_test.cc',
        'platform/test/ozone_platform_test.h',
        '<@(external_ozone_platform_files)',
      ],
      'includes': [
        'ime/ime.gypi',
        'platform/caca/caca.gypi',
      ],
      'actions': [
        {
          'action_name': 'generate_ozone_platform_list',
          'variables': {
            'generator_path': 'generate_ozone_platform_list.py',
          },
          'inputs': [
            '<(generator_path)',
          ],
          'outputs': [
            '<(platform_list_file)',
          ],
          'action': [
            'python',
            '<(generator_path)',
            '--output_file=<(platform_list_file)',
            '--default=<(ozone_platform)',
            '<@(ozone_platforms)',
          ],
        },
      ],
      'conditions': [
        ['<(ozone_platform_dri)==1', {
          'variables': {
            'ozone_platforms': [
              'dri'
            ]
          }
        }, {  # ozone_platform_dri==0
          'sources/': [
            ['exclude', '^platform/dri/'],
          ]
        }],
        ['<(ozone_platform_test)==1', {
          'variables': {
            'ozone_platforms': [
              'test'
            ],
          }
        }, {  # ozone_platform_test==0
          'sources/': [
            ['exclude', '^platform/test/'],
          ]
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/ui/display/display.gyp:display_types',
          ],
        }],
      ]
    },
  ],
}
