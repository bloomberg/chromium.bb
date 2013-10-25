# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
        {
          'action_name': 'webui_resources',
          'variables': {
            'grit_grd_file': 'webui/resources/webui_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'ui_unscaled_resources',
          'variables': {
            'grit_grd_file': 'resources/ui_unscaled_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}
