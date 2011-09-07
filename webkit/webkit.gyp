# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/win_precompile.gypi',
    '../third_party/WebKit/Source/WebKit/chromium/features.gypi',
    'tools/test_shell/test_shell.gypi',
  ],
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'pull_in_webkit_unit_tests',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit_unit_tests'
      ],
    },
    {
      'target_name': 'pull_in_DumpRenderTree',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:DumpRenderTree'
      ],
    },
  ], # targets
}
