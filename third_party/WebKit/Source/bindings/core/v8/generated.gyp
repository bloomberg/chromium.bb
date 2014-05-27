# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate IDL bindings for core, plus aggregate bindings files.
#
# Design doc: http://www.chromium.org/developers/design-documents/idl-build

{
  'includes': [
    'generated.gypi',
    '../../idl.gypi',
    '../../scripts/scripts.gypi',
  ],

  'targets': [
################################################################################
  {
    'target_name': 'bindings_core_generated_aggregate',
    'type': 'none',
    'actions': [{
      'action_name': 'generate_aggregate_bindings_core',
      'inputs': [
        '<(bindings_scripts_dir)/aggregate_generated_bindings.py',
        '<(core_idl_files_list)',
      ],
      'outputs': [
        '<@(bindings_core_generated_aggregate_files)',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/aggregate_generated_bindings.py',
        '<(core_idl_files_list)',
        '--',
        '<@(bindings_core_generated_aggregate_files)',
      ],
      'message': 'Generating aggregate generated core bindings files',
    }],
  },
################################################################################
  {
    'target_name': 'bindings_core_generated',
    'type': 'none',
    'dependencies': [
      'bindings_core_generated_aggregate',
      # FIXME: move individual bindings here http://crbug.com/358074
      # 'bindings_core_generated_individual',
    ],
  },
################################################################################
  ],  # targets
}
