# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ui_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources',
      },
      'actions': [
        {
          'action_name': 'ui_resources',
          'variables': {
            'grit_grd_file': 'resources/ui_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'ui_resources_large',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_large',
      },
      'actions': [
        {
          'action_name': 'ui_resources_large',
          'variables': {
            'grit_grd_file': 'resources/ui_resources_large.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'ui_resources_standard',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard',
      },
      'actions': [
        {
          'action_name': 'ui_resources_standard',
          'variables': {
            'grit_grd_file': 'resources/ui_resources_standard.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}
