# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'setup_third_party_cmd': ['python', 'setup_third_party.py'],
    'webkit_client_api_dest': '<(SHARED_INTERMEDIATE_DIR)/webkit/third_party/WebKit/Source/WebKit/chromium/public',
    'platform_api_dest': '<(SHARED_INTERMEDIATE_DIR)/webkit/third_party/WebKit/Source/Platform/chromium/public',
    'mac_webcoresupport_dest': '<(SHARED_INTERMEDIATE_DIR)/webkit/third_party/WebKit/Source/WebKit/mac/WebCoreSupport',
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
          'action_name': 'third_party_webkit_client_api_forwarding_headers',
          'inputs': [
            '<!@(<(setup_third_party_cmd) inputs <(DEPTH)/public)',
            'setup_third_party.py',
          ],
          'outputs': [
            "<!@(<(setup_third_party_cmd) outputs <(DEPTH)/public '<(webkit_client_api_dest)')",
          ],
          'action': [
            '<@(setup_third_party_cmd)',
            'setup_headers',
            '<(DEPTH)/public',
            '<(webkit_client_api_dest)',
          ],
          'message': 'Generating forwarding headers for third_party/WebKit/Source/WebKit/chromium/public',
        },
        {
        'action_name': 'third_party_platform_api_forwarding_headers',
          'inputs': [
            '<!@(<(setup_third_party_cmd) inputs <(DEPTH)/../../Platform/chromium/public)',
            'setup_third_party.py',
          ],
          'outputs': [
            "<!@(<(setup_third_party_cmd) outputs <(DEPTH)/../../Platform/chromium/public '<(platform_api_dest)')",
          ],
          'action': [
            '<@(setup_third_party_cmd)',
            'setup_headers',
            '<(DEPTH)/../../Platform/chromium/public',
            '<(platform_api_dest)',
            '<(DEPTH)',
          ],
          'message': 'Generating forwarding headers for third_party/WebKit/Source/Platform/chromium/public',
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
                "<!@(<(setup_third_party_cmd) outputs <(DEPTH)/../mac/WebCoreSupport '<(mac_webcoresupport_dest)')",
              ],
              'action': [
                '<@(setup_third_party_cmd)',
                'setup_headers',
                '<(DEPTH)/../mac/WebCoreSupport',
                '<(mac_webcoresupport_dest)',
              ],
              'message': 'Generating forwarding headers for third_party/WebKit/Source/WebKit/mac/WebCoreSupport',
            },
          ],
        }],
      ],
    },
  ],
}
