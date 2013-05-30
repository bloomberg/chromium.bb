# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'pull_in_webkit_unit_tests',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKitUnitTests.gyp:webkit_unit_tests'
      ],
    },
    {
      'target_name': 'pull_in_DumpRenderTree',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp:DumpRenderTree'
      ],
    },
  ],
  'conditions': [
    # Currently test_shell compiles only on Windows, Mac, and Gtk.
    ['OS=="win" or OS=="mac" or toolkit_uses_gtk==1', {
      'targets': [
        {
          # TODO(darin): Delete this dummy target once the build masters stop
          # trying to build it.
          'target_name': 'test_shell',
          'type': 'static_library',
          'sources': [
            'support/test_shell_dummy.cc',
          ],
        },
      ],
    }],
  ],
}
