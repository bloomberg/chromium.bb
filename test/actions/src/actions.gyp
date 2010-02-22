# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pull_in_all_actions',
      'type': 'none',
      'dependencies': [
        'subdir1/executable.gyp:*',
        'subdir2/none.gyp:*',
        'subdir3/null_input.gyp:*',
      ],
    },
    {
      'target_name': 'depend_on_always_run_action',
      'type': 'none',
      'actions': [
        {
          'action_name': 'use_always_run_output',
          'inputs': [
            'subdir1/actions-out/action-counter.txt',
            'subdir1/counter.py',
          ],
          'outputs': [
            'subdir1/actions-out/action-counter_2.txt',
          ],
          'action': [
            'python', 'subdir1/counter.py', '<(_outputs)',
          ],
          # Allows the test to run without hermetic cygwin on windows.
          'msvs_cygwin_shell': 0,
        },
      ],
    },
  ],
}
