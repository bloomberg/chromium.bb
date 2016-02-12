# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
  },

  'targets': [
    {
      # GN version: //third_party/WebKit/Source/platform/inspector_protocol_sources
      'target_name': 'protocol_sources',
      'type': 'none',
      'dependencies': [
        'protocol_version'
      ],
      'actions': [
        {
          'action_name': 'generateInspectorProtocolBackendSources',
          'inputs': [
            # The python script in action below.
            'CodeGenerator.py',
            # The helper script imported by CodeGenerator.py.
            'CodeGeneratorStrings.py',
            # Input file for the script.
            '../../devtools/protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/inspector_protocol/Dispatcher.cpp',
            '<(blink_platform_output_dir)/inspector_protocol/Dispatcher.h',
            '<(blink_platform_output_dir)/inspector_protocol/Frontend.cpp',
            '<(blink_platform_output_dir)/inspector_protocol/Frontend.h',
            '<(blink_platform_output_dir)/inspector_protocol/TypeBuilder.cpp',
            '<(blink_platform_output_dir)/inspector_protocol/TypeBuilder.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            'CodeGenerator.py',
            '../../devtools/protocol.json',
            '--output_dir', '<(blink_platform_output_dir)/inspector_protocol',
          ],
          'message': 'Generating Inspector protocol backend sources from protocol.json',
        },
      ]
    },
    {
      # GN version: //third_party/WebKit/Source/platform/inspector_protocol_version
      'target_name': 'protocol_version',
      'type': 'none',
      'actions': [
         {
          'action_name': 'generateInspectorProtocolVersion',
          'inputs': [
            'generate-inspector-protocol-version',
            '../../devtools/protocol.json',
          ],
          'outputs': [
            '<(blink_platform_output_dir)/inspector_protocol/InspectorProtocolVersion.h',
          ],
          'variables': {
            'generator_include_dirs': [
            ],
          },
          'action': [
            'python',
            'generate-inspector-protocol-version',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
          'message': 'Validate inspector protocol for backwards compatibility and generate version file',
        }
      ]
    },
  ],  # targets
}
