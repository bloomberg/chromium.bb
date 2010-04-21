# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'list_headers_cmd': ['python', 'list_headers.py'],
    'destination': '<(DEPTH)/third_party/WebKit/WebKit/chromium/public',
  },
  'targets': [
    {
      # TODO(tony): Would be nice if this would make symlinks on linux/mac
      # and try to make hardlinks on ntfs.
      'target_name': 'third_party_headers',
      'type': 'none',
      'copies': [
        {
          'destination': '<(destination)',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/)',
          ],
        },
        {
          'destination': '<(destination)/gtk',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/gtk/)',
          ],
        },
        {
          'destination': '<(destination)/linux',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/linux/)',
          ],
        },
        {
          'destination': '<(destination)/mac',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/mac/)',
          ],
        },
        {
          'destination': '<(destination)/win',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/win/)',
          ],
        },
        {
          'destination': '<(destination)/x11',
          'files': [
            '<!@(<(list_headers_cmd) <(DEPTH)/public/x11/)',
          ],
        },
        {
          'destination': '<(DEPTH)/third_party/WebKit/WebKit/mac/WebCoreSupport',
          'files': [
            '<(DEPTH)/../mac/WebCoreSupport/WebSystemInterface.h',
          ],
        },
      ]
    },
  ],
}
