# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    # Set this to run the jscompile checks after building the webapp.
    'run_jscompile%': 1,

    # Set this to enable cast mode on the android client.
    'enable_cast%': 0,

    'variables': {
      'conditions': [
        # Enable the multi-process host on Windows by default.
        ['OS=="win"', {
          'remoting_multi_process%': 1,
        }, {
          'remoting_multi_process%': 0,
        }],
      ],
    },
    'remoting_multi_process%': '<(remoting_multi_process)',

    'remoting_rdp_session%': 1,

    'branding_path': '../remoting/branding_<(branding)',

    'conditions': [
      ['OS=="win"', {
        # Java is not available on Windows bots, so we need to disable
        # JScompile checks.
        'run_jscompile': 0,
      }],
    ],
  },

}
