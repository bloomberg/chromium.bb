# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'blink_platform_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/platform',
    'jinja_module_files': [
      # jinja2/__init__.py contains version string, so sufficient for package
      '<(DEPTH)/third_party/jinja2/__init__.py',
      '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
    ],
  },
  'targets': [
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
