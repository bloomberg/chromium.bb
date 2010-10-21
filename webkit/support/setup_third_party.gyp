# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'setup_third_party_cmd': ['python', 'setup_third_party.py'],
    'destination': '<(SHARED_INTERMEDIATE_DIR)/webkit/third_party/WebKit/WebKit/chromium/public',
    'destination_mac': '<(SHARED_INTERMEDIATE_DIR)/webkit/third_party/WebKit/WebKit/mac/WebCoreSupport',
  },
  'targets': [
    {
      'target_name': 'third_party_headers',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/webkit',
        ],
      },
      'actions': [
        {
          'action_name': 'third_party_forwarding_headers',
          'inputs': [
            '<!@(<(setup_third_party_cmd) inputs <(DEPTH)/public)',
            'setup_third_party.py',
          ],
          'outputs': [
            "<!@(<(setup_third_party_cmd) outputs <(DEPTH)/public '<(destination)')",
          ],
          'action': [
            '<@(setup_third_party_cmd)',
            'setup_headers',
            '<(DEPTH)/public',
            '<(destination)',
          ],
          'message': 'Generating forwarding headers for third_party/WebKit/WebKit/chromium/public',
        },
      ],
      'conditions': [
        ['OS=="mac"', {
          'actions': [
            {
              'action_name': 'third_party_mac_forwarding_headers',
              'inputs': [
                '<!@(<(setup_third_party_cmd) inputs <(DEPTH)/../mac/WebCoreSupport)',
                'setup_third_party.py',
              ],
              'outputs': [
                "<!@(<(setup_third_party_cmd) outputs <(DEPTH)/../mac/WebCoreSupport '<(destination_mac)')",
              ],
              'action': [
                '<@(setup_third_party_cmd)',
                'setup_headers',
                '<(DEPTH)/../mac/WebCoreSupport',
                '<(destination_mac)',
              ],
              'message': 'Generating forwarding headers for third_party/WebKit/WebKit/mac/WebCoreSupport',
            },
          ],
        }],
      ],
    },
  ],
}
