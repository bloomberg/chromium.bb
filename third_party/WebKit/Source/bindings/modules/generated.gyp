# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate IDL interfaces info for modules, used to generate bindings
#
# Design doc: http://www.chromium.org/developers/design-documents/idl-build

{
  'includes': [
    '../../build/scripts/scripts.gypi',
    '../bindings.gypi',
    '../core/core.gypi',
    '../scripts/scripts.gypi',
    'idl.gypi',
    'modules.gypi',
  ],

  'targets': [
################################################################################
  {
    'target_name': 'bindings_modules_generated',
    'type': 'none',
    'actions': [
      {
        'action_name': 'event_interfaces',
        'variables': {
          'event_idl_files': [
            '<@(modules_event_idl_files)',
          ],
          'event_idl_files_list':
              '<|(event_idl_files_list.tmp <@(event_idl_files))',
        },
        'inputs': [
          '<(bindings_scripts_dir)/generate_event_interfaces.py',
          '<(bindings_scripts_dir)/utilities.py',
          '<(event_idl_files_list)',
          '<@(event_idl_files)',
        ],
        'outputs': [
          '<(blink_output_dir)/EventModulesInterfaces.in',
        ],
        'action': [
          'python',
          '<(bindings_scripts_dir)/generate_event_interfaces.py',
          '--event-idl-files-list',
          '<(event_idl_files_list)',
          '--event-interfaces-file',
          '<(blink_output_dir)/EventModulesInterfaces.in',
          '--write-file-only-if-changed',
          '<(write_file_only_if_changed)',
          '--suffix',
          'Modules',
        ],
      },
      {
        'action_name': 'EventModulesFactory',
        'inputs': [
          '<@(make_event_factory_files)',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesInterfaces.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModules.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesHeaders.h',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesInterfaces.h',
        ],
        'action': [
          'python',
          '../../build/scripts/make_event_factory.py',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesInterfaces.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
      {
        'action_name': 'EventModulesNames',
        'inputs': [
          '<@(make_names_files)',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesInterfaces.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesNames.h',
        ],
        'action': [
          'python',
          '../../build/scripts/make_names.py',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventModulesInterfaces.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
      {
        'action_name': 'EventTargetModulesFactory',
        'inputs': [
          '<@(make_event_factory_files)',
          '../../modules/EventTargetModulesFactory.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesHeaders.h',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesInterfaces.h',
        ],
        'action': [
          'python',
          '../../build/scripts/make_event_factory.py',
          '../../modules/EventTargetModulesFactory.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
      {
        'action_name': 'EventTargetModulesNames',
        'inputs': [
          '<@(make_names_files)',
          '../../modules/EventTargetModulesFactory.in',
        ],
        'outputs': [
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesNames.cpp',
          '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetModulesNames.h',
        ],
        'action': [
          'python',
          '../../build/scripts/make_names.py',
          '../../modules/EventTargetModulesFactory.in',
          '--output_dir',
          '<(SHARED_INTERMEDIATE_DIR)/blink',
        ],
      },
    ],
  },
  {
    'target_name': 'interfaces_info_individual_modules',
    'type': 'none',
    'dependencies': [
      # FIXME: should be modules_generated_idls
      # http://crbug.com/358074
      '../generated.gyp:generated_idls',
    ],
    'actions': [{
      'action_name': 'compute_interfaces_info_individual_modules',
      'inputs': [
        '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
        '<(bindings_scripts_dir)/utilities.py',
        '<(modules_static_idl_files_list)',
        '<@(modules_static_idl_files)',
        # No generated files currently, will add with constructors
        # '<@(modules_generated_idl_files)',
      ],
      'outputs': [
        '<(bindings_modules_output_dir)/InterfacesInfoModulesIndividual.pickle',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/compute_interfaces_info_individual.py',
        '--component-dir',
        'modules',
        '--idl-files-list',
        '<(modules_static_idl_files_list)',
        '--interfaces-info-file',
        '<(bindings_modules_output_dir)/InterfacesInfoModulesIndividual.pickle',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        # No generated files currently, will add with constructors
        # '--',
        # Generated files must be passed at command line
        # '<@(modules_generated_idl_files)',
      ],
      'message': 'Computing global information about individual IDL files',
      }]
  },
################################################################################
  {
    'target_name': 'interfaces_info',
    'type': 'none',
    'dependencies': [
        '../core/generated.gyp:interfaces_info_individual_core',
        'interfaces_info_individual_modules',
    ],
    'actions': [{
      'action_name': 'compute_interfaces_info_overall',
      'inputs': [
        '<(bindings_scripts_dir)/compute_interfaces_info_overall.py',
        '<(bindings_core_output_dir)/InterfacesInfoCoreIndividual.pickle',
        '<(bindings_modules_output_dir)/InterfacesInfoModulesIndividual.pickle',
      ],
      'outputs': [
        '<(bindings_modules_output_dir)/InterfacesInfoModules.pickle',
      ],
      'action': [
        'python',
        '<(bindings_scripts_dir)/compute_interfaces_info_overall.py',
        '--write-file-only-if-changed',
        '<(write_file_only_if_changed)',
        '--',
        '<(bindings_core_output_dir)/InterfacesInfoCoreIndividual.pickle',
        '<(bindings_modules_output_dir)/InterfacesInfoModulesIndividual.pickle',
        '<(bindings_modules_output_dir)/InterfacesInfoModules.pickle',
      ],
      'message': 'Computing overall global information about IDL files',
      }]
  },
################################################################################
  ],  # targets
}
