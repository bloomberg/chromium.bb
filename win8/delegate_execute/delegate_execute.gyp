# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'delegate_execute',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../chrome/chrome.gyp:installer_util',
        '../../google_update/google_update.gyp:google_update',
        '../../win8/win8.gyp:check_sdk_patch',
      ],
      'sources': [
        'chrome_util.cc',
        'chrome_util.h',
        'command_execute_impl.cc',
        'command_execute_impl.h',
        'command_execute_impl.rgs',
        'delegate_execute.cc',
        'delegate_execute.rc',
        'delegate_execute.rgs',
        'delegate_execute_operation.cc',
        'delegate_execute_operation.h',
        'resource.h',
      ],
      'defines': [
        # This define is required to pull in the new Win8 interfaces from
        # system headers like ShObjIdl.h
        'NTDDI_VERSION=0x06020000',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
    },
  ],
}
