# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate IDL interfaces info for core, used to generate bindings
#
# Design doc: http://www.chromium.org/developers/design-documents/idl-build

{
  'includes': [
    # ../.. == Source
    '../../bindings/bindings.gypi',
    '../../bindings/scripts/scripts.gypi',
    '../../core/core.gypi',
    'core.gypi',
    'generated.gypi',
    'idl.gypi',
  ],

  'targets': [
################################################################################
  {
    'target_name': 'core_global_constructors_idls',
    'type': 'none',
    'dependencies': [
        # FIXME: should be core_global_objects http://crbug.com/358074
        '../generated.gyp:global_objects',
    ],
    'actions': [{
      'action_name': 'generate_core_global_constructors_idls',
      'inputs': [
        '<(bindings_scripts_dir)/generate_global_constructors.py',
        '<(bindings_scripts_dir)/utilities.py',
        # Only includes main IDL files (exclude dependencies and testing,
        # which should not appear on global objects).
        '<(core_idl_files_list)',
        '<@(core_idl_files)',
        '<(bindings_output_dir)/GlobalObjects.pickle',
      ],
      'outputs': [
        '<@(core_global_constructors_generated_idl_files)',
        '<@(core_global_constructors_generated_header_files)',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/generate_global_constructors.py',
        '--idl-files-list',
        '<(core_idl_files_list)',
        '--global-objects-file',
        '<(bindings_output_dir)/GlobalObjects.pickle',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '--',
        'Window',
        '<(blink_core_output_dir)/WindowCoreConstructors.idl',
        'SharedWorkerGlobalScope',
        '<(blink_core_output_dir)/SharedWorkerGlobalScopeCoreConstructors.idl',
        'DedicatedWorkerGlobalScope',
        '<(blink_core_output_dir)/DedicatedWorkerGlobalScopeCoreConstructors.idl',
        'ServiceWorkerGlobalScope',
        '<(blink_core_output_dir)/ServiceWorkerGlobalScopeCoreConstructors.idl',
       ],
       'message':
         'Generating IDL files for constructors on global objects from core',
      }]
  },
################################################################################
  {
    'target_name': 'interfaces_info_individual_core',
    'type': 'none',
    'dependencies': [
      '../../core/core_generated.gyp:generated_testing_idls',
      'core_global_constructors_idls',
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
        '<(bindings_core_output_dir)/InterfacesInfoCoreIndividual.pickle',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
        '--component-dir',
        'core',
        '--idl-files-list',
        '<(core_static_idl_files_list)',
        '--interfaces-info-file',
        '<(bindings_core_output_dir)/InterfacesInfoCoreIndividual.pickle',
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
