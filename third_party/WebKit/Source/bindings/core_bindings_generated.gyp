# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate EventInterfaces.in, used by core/ but depends on modules/,
# hence placed in bindings/ to avoid direct core/ -> modules/ dependency.

{
  'includes': [
    'bindings.gypi',
    '../core/core.gypi',
    '../modules/modules.gypi',
  ],

  'targets': [
  {
    'target_name': 'core_bindings_generated',
    'type': 'none',
    'actions': [
      {
        'action_name': 'event_interfaces',
        'variables': {
          'event_idl_files': [
            '<@(core_event_idl_files)',
            '<@(modules_event_idl_files)',
          ],
          'event_idl_files_list':
              '<|(event_idl_files_list.tmp <@(event_idl_files))',
        },
        'inputs': [
          '../bindings/scripts/generate_event_interfaces.py',
          '../bindings/scripts/utilities.py',
          '<(event_idl_files_list)',
          '<@(event_idl_files)',
        ],
        'outputs': [
          '<(blink_output_dir)/EventInterfaces.in',
        ],
        'action': [
          'python',
          '../bindings/scripts/generate_event_interfaces.py',
          '--event-idl-files-list',
          '<(event_idl_files_list)',
          '--event-interfaces-file',
          '<(blink_output_dir)/EventInterfaces.in',
          '--write-file-only-if-changed',
          '<(write_file_only_if_changed)',
        ],
      },
    ],
  },
  ],  # targets
}
