# Copyright 2010, Google Inc.
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

{
  # ----------------------------------------------------------------------
  # Default settings
  # ----------------------------------------------------------------------

  'includes': [
    '../../../build/common.gypi',
  ],
  'variables': {
    'validate_gen_out':
      '<(SHARED_INTERMEDIATE_DIR)/gen/native_client/src/trusted/validator_arm',
  },
  # TODO(robertm): move these setting to some global config
  'target_defaults': {
    'cflags!': [
      '-Wextra',
      '-Wswitch-enum',
      '-Wsign-compare'
    ],
    'xcode_settings': {
      'WARNING_CFLAGS!': [
        '-Wextra',
        '-Wswitch-enum',
        '-Wsign-compare'
      ]
  },
  'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
    # TODO(robertm): mmentovai does not think this is necessary
    # When ncvalidate is a dependency, it needs to be a hard dependency
    # because dependents may rely on ncvalidate to create header files below.
    'hard_dependency': 1,
    },
  # ----------------------------------------------------------------------
  # actual targets
  # ----------------------------------------------------------------------
    'targets': [
    # ----------------------------------------------------------------------
    {
      'target_name': 'arm_validator_core',
      'type': 'static_library',
      'sources': [
        'address_set.cc',
        'inst_classes.cc',
        'validator.cc',
        '<(validate_gen_out)/decode.cc'
      ],
      'dependencies': ['decode_gen'],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'ncvalidate_arm_v2',
      'type': 'static_library',
      'sources': [ 'ncvalidate.cc' ],
      'dependencies': [
        'arm_validator_core'
      ],
    },
    # ----------------------------------------------------------------------
    {
      'target_name': 'decode_gen',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
      'actions': [
       {
          'action_name': 'decode_gen_action',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'armv7-opt.table',
            'generate_decoder.py',
            'dgen_core.py',
            'dgen_input.py',
            'dgen_opt.py',
            'dgen_output.py',
          ],
          'outputs': [
            '<(validate_gen_out)/decode.cc',
          ],
          'action': [
            '<@(python_exe)',
            'generate_decoder.py',
            'armv7-opt.table',
            '<@(_outputs)',
           ],
          'process_outputs_as_sources': 1,
          'message': 'generate decoder.cc',
        },
      ],
    },
  ],
}
