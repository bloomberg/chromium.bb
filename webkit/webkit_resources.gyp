# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
# TODO(jamesr): These targets should be called webkit_(resources|strings), but can't
# until blink is updated.
    {
      'target_name': 'webkit_temp_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_temp_resources',
          'variables': {
            'grit_grd_file': 'glue/resources/webkit_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'blink_chromium_resources',
          'variables': {
            'grit_grd_file': '../third_party/WebKit/Source/WebKit/chromium/WebKit.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [ '<(grit_out_dir)' ],
      },
    },
    {
      'target_name': 'webkit_temp_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_temp_strings',
          'variables': {
            'grit_grd_file': 'glue/webkit_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ]
}

