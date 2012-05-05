# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    # We get "test_arg" via a .gypi file in order to test that the
    # action gets rerun when the .gypi file changed even if the .gyp
    # file does not change.
    'arg-action.gypi',
  ],
  'targets': [
    {
      'target_name': 'action_using_arg',
      'type': 'none',
      'msvs_cygwin_shell': 0,
      'actions': [
        {
          'action_name': 'action_using_arg',
          # We have to have at least one input file to reproduce the
          # problem in the unfixed version of Gyp.
          'inputs': [
            'dummy-fixed-input.txt',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/dest-file.txt',
          ],
          'action': [
            'python', 'write_file.py', '<@(_outputs)', '<(test_arg)',
          ],
        },
      ],
    },
  ],
}
