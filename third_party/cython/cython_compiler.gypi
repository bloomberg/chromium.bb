# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'python_flags': '<(DEPTH)/third_party/cython/python_flags.py',
  },
  'conditions': [
    ['OS=="mac"', {
      'variables': {
        'module_prefix': '',
        'module_suffix': '.so',
      },
    }, {
      'variables': {
        'module_prefix': '<(SHARED_LIB_PREFIX)',
        'module_suffix': '<(SHARED_LIB_SUFFIX)',
      },
    }],
  ],
  'type': 'loadable_module',
  'rules': [
    {
      'rule_name': '<(_target_name)_cython_compiler',
      'extension': 'pyx',
      'variables': {
        'cython_compiler': '<(DEPTH)/third_party/cython/src/cython.py',
      },
      'inputs': [
        '<(cython_compiler)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/cython/<(python_base_module)/<(RULE_INPUT_ROOT).cc',
      ],
      'action': [
        'python', '<(cython_compiler)',
        '--cplus',
        '-I<(DEPTH)',
        '-o', '<@(_outputs)',
        '<(RULE_INPUT_PATH)',
      ],
      'message': 'Generating C++ source from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    }
  ],
  'include_dirs': [
    '<!@(python <(python_flags) --includes)',
    '<(DEPTH)',
  ],
  'libraries': [
    '<!@(python <(python_flags) --libraries)',
  ],
  'cflags': [
    '-Wno-unused-function',
  ],
  'xcode_settings': {
    'WARNING_CFLAGS': [ '-Wno-unused-function' ],
  },
  'library_dirs': [
    '<!@(python <(python_flags) --library_dirs)',
  ],
  'direct_dependent_settings': {
    'variables': {
      'python_binary_modules': [
        '<(python_cython_module)<(module_suffix)=<(module_prefix)<(_target_name)<(module_suffix)',
      ],
    },
  },
  'hard_dependency': 1,
}
