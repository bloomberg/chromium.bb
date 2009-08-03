# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'split_samples',
      'type': 'none',
      'rules': [
        {
          'rule_name': 'split_sample',
          'extension': 'html',
          'inputs': [
            'split_samples.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/samples/<(RULE_INPUT_NAME)',
            '<(PRODUCT_DIR)/samples/<(RULE_INPUT_ROOT).js',
          ],
          'action': ['python', '<@(_inputs)',
                     '--products', '<(PRODUCT_DIR)/samples',
                     '--samples', '.',
                     '<(RULE_INPUT_PATH)',
          ],
        },
      ],
      'sources': [
        '<!@(python split_samples.py --samples . --find_candidates)',
      ],
    },
    {
      'target_name': 'samples',
      'type': 'none',
      'dependencies': [
        'split_samples',
        '<!(python samples_gen.py):build_samples',
      ],
    },
  ],
}
