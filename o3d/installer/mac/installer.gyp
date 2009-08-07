# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'nppversion': '<!(python ../../plugin/version_info.py --commaversion)',
    'dotnppversion': '<!(python ../../plugin/version_info.py --version)',

    # We don't actually want the extras version to update by itself;
    # it should change only when we actually add something to the
    # installer or change the d3dx9 version.  This version is
    # therefore independent of the o3d plugin and sdk versions.
    'extrasversion': '0,1,1,0',
    'dotextrasversion': '0.1.1.0',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'installer',
      'type': 'none',
      'dependencies': [
        '../../converter/converter.gyp:o3dConverter',
        '../../breakpad/breakpad.gyp:reporter',
        '../../google_update/google_update.gyp:getextras',
        '../../documentation/documentation.gyp:*',
        '../../plugin/plugin.gyp:npo3dautoplugin',
        '../../samples/samples.gyp:samples',
      ],
    },
  ],
}
