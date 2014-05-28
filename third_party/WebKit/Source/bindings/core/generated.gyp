# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate IDL interfaces info for core, used to generate bindings
#
# Design doc: http://www.chromium.org/developers/design-documents/idl-build

{
  'includes': [
    '../bindings.gypi',
    '../scripts/scripts.gypi',
    'idl.gypi',
  ],

  'targets': [
################################################################################
  {
    'target_name': 'interfaces_info_individual_core',
    'type': 'none',
    'dependencies': [
      # FIXME: should be core_generated_idls
      # http://crbug.com/358074
      '../generated.gyp:generated_idls',
    ],
    'actions': [{
      'action_name': 'compute_interfaces_info_individual_core',
      'inputs': [
        '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
        '<(bindings_scripts_dir)/utilities.py',
        '<(core_static_idl_files_list)',
        '<@(core_static_idl_files)',
        '<@(core_generated_idl_files)',
      ],
      'outputs': [
        '<(blink_output_dir)/InterfacesInfoCoreIndividual.pickle',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
        '--idl-files-list',
        '<(core_static_idl_files_list)',
        '--interfaces-info-file',
        '<(blink_output_dir)/InterfacesInfoCoreIndividual.pickle',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '--',
        # Generated files must be passed at command line
        '<@(core_generated_idl_files)',
      ],
      'message': 'Computing global information about individual IDL files',
      }]
  },
################################################################################
  ],  # targets
}
