# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //webkit/glue/resources
      'target_name': 'webkit_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'generate_webkit_resources',
          'variables': {
            'grit_grd_file': 'webkit_resources.grd',
          },
          'includes': [ '../../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../../build/grit_target.gypi' ],
    },
  ]
}
