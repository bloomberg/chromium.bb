# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'check_sdk_patch',
      'type': 'none',
      'variables': {
        'check_sdk_script': 'util/check_sdk_patch.py',
        'output_path': '<(INTERMEDIATE_DIR)/check_sdk_patch',
      },
      'actions': [
        {
          'action_name': 'check_sdk_patch_action',
          'inputs': [
            '<(check_sdk_script)',
          ],
          'outputs': [
            # This keeps the ninja build happy and provides a slightly helpful
            # error messge if the sdk is missing.
            '<(output_path)'
          ],
          'action': ['python',
                     '<(check_sdk_script)',
                     '<(windows_sdk_path)',
                     '<(output_path)',
                     ],
        },
      ],
    },
    {
      'target_name': 'win8_util',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'util/win8_util.cc',
        'util/win8_util.h',
      ],
    },
    {
      'target_name': 'test_support_win8',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'test/open_with_dialog_async.cc',
        'test/open_with_dialog_async.h',
        'test/open_with_dialog_controller.cc',
        'test/open_with_dialog_controller.h',
        'test/ui_automation_client.cc',
        'test/ui_automation_client.h',
      ],
    },
  ],
}
