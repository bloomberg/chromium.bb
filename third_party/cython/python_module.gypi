# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'python_binary_modules%': [],
    'python_module_destination': '<(PRODUCT_DIR)/python/<(python_base_module)',
    'cp': '<(DEPTH)/third_party/cython/cp_python_binary_modules.py',
    'timestamp': '<(SHARED_INTERMEDIATE_DIR)/<(_target_name)_py_module.stamp',
  },
  'rules': [
    {
      'rule_name': '<(_target_name)_cp_python',
      'extension': 'py',
      'inputs': [
        '<(DEPTH)/build/cp.py',
      ],
      'outputs': [
        '<(python_module_destination)/<(RULE_INPUT_NAME)',
      ],
      'action': [
        'python',
        '<@(_inputs)',
        '<(RULE_INPUT_PATH)',
        '<@(_outputs)',
      ],
      'message': 'Moving <(RULE_INPUT_PATH) to its destination',
    }
  ],
  'actions': [
    {
      'action_name': '(_target_name)_move_to_python_modules',
      'inputs': [
        '<(cp)',
      ],
      'outputs': [
        '<(timestamp)',
      ],
      'action': [
        'python',
        '<(cp)',
        '<(timestamp)',
        '<(PRODUCT_DIR)',
        '<(python_module_destination)',
        '>@(python_binary_modules)',
      ],
    },
  ],
  'direct_dependent_settings': {
    'variables': {
      'python_module_timestamps': [
        '<(timestamp)',
      ],
    },
  },
  'hard_dependency': 1,
}
