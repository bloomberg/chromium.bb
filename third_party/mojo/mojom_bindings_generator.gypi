# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojom_bindings_generator_variables.gypi',
  ],
  'actions': [
    {
      'variables': {
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'stamp_filename': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/<(_target_name).stamp',
      },
      'action_name': '<(_target_name)_mojom_bindings_stamp',
      # The java output directory is deleted to ensure that the java library
      # doesn't try to compile stale files.
      'action': [
        'python', '<(DEPTH)/build/rmdir_and_stamp.py',
        '<(java_out_dir)',
        '<(stamp_filename)',
      ],
      'inputs': [ '<@(_sources)' ],
      'outputs': [ '<(stamp_filename)' ],
    }
  ],
  'rules': [
    {
      'rule_name': '<(_target_name)_mojom_bindings_generator',
      'extension': 'mojom',
      'variables': {
        'mojom_base_output_dir':
            '<!(python <(DEPTH)/build/inverse_depth.py <(DEPTH))',
        'java_out_dir': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
        'mojom_import_args%': [
         '-I<(DEPTH)',
         '-I<(DEPTH)/third_party/mojo/src'
        ],
        'stamp_filename': '<(PRODUCT_DIR)/java_mojo/<(_target_name)/<(_target_name).stamp',
      },
      'inputs': [
        '<@(mojom_bindings_generator_sources)',
        '<(stamp_filename)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.cc',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.h',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom.js',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)_mojom.py',
        '<(SHARED_INTERMEDIATE_DIR)/<(mojom_base_output_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom-internal.h',
      ],
      'action': [
        'python', '<@(mojom_bindings_generator)',
        './<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
        '--use_bundled_pylibs',
        '-d', '<(DEPTH)',
        '<@(mojom_import_args)',
        '-o', '<(SHARED_INTERMEDIATE_DIR)',
        '--java_output_directory=<(java_out_dir)',
      ],
      'message': 'Generating Mojo bindings from <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).mojom',
      'process_outputs_as_sources': 1,
    }
  ],
  'include_dirs': [
    '<(DEPTH)',
    '<(DEPTH)/third_party/mojo/src',
    '<(SHARED_INTERMEDIATE_DIR)',
    '<(SHARED_INTERMEDIATE_DIR)/third_party/mojo/src',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(DEPTH)',
      '<(DEPTH)/third_party/mojo/src',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(SHARED_INTERMEDIATE_DIR)/third_party/mojo/src',
    ],
    'variables': {
      'generated_src_dirs': [
        '<(PRODUCT_DIR)/java_mojo/<(_target_name)/src',
      ],
      'additional_input_paths': [
        '<@(mojom_bindings_generator_sources)',
        '<@(_sources)',
      ],
    },
  },
  'hard_dependency': 1,
}
