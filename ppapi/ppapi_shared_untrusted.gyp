# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
    'ppapi_shared.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_shared_untrusted',
          'type': 'none',
          'variables': {
            'ppapi_shared_target': 1,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libppapi_shared_untrusted.a',
            'build_glibc': 0,
            'build_newlib': 1,
          },
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
            '../base/base_untrusted.gyp:base_untrusted',
            '../ipc/ipc_untrusted.gyp:ipc_untrusted',
          ],
        },
      ],
    }],
  ],
}
