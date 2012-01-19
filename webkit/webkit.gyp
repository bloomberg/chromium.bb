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
  'conditions': [
    # TODO(dpranke): See https://bugs.webkit.org/show_bug.cgi?id=68463 , 
    # flag in build/common.gypi.
    ['build_webkit_exes_from_webkit_gyp==1', {
      'targets': [
        {
          'target_name': 'pull_in_webkit_unit_tests',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit_unit_tests'
          ],
        },
        {
          'target_name': 'pull_in_copy_TestNetscapePlugIn',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNestcapePlugIn'
          ],
        },
        {
          'target_name': 'pull_in_DumpRenderTree',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:DumpRenderTree'
          ],
        },
      ],
    }, {  # build_webkit_exes_from_webkit_gyp==0
      'targets': [
        {
          'target_name': 'pull_in_webkit_unit_tests',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/Source/WebKit/chromium/WebKitUnitTests.gyp:webkit_unit_tests'
          ],
        },
        {
          'target_name': 'pull_in_copy_TestNetscapePlugIn',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp:copy_TestNetscapePlugIn'
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
    }]
  ], # targets
}
