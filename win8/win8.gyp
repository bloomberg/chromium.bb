# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_support_win8',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'test_registrar_constants',
      ],
      'sources': [
        'test/open_with_dialog_async.cc',
        'test/open_with_dialog_async.h',
        'test/open_with_dialog_controller.cc',
        'test/open_with_dialog_controller.h',
        'test/ui_automation_client.cc',
        'test/ui_automation_client.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'test_registrar_constants',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/test_registrar_constants.cc',
        'test/test_registrar_constants.h',
      ],
    },
    {
      'target_name': 'visual_elements_resources',
      'type': 'none',
      'copies': [
        {
          # GN version: //win8/visual_elements_resources
          'destination': '<(PRODUCT_DIR)',
          'files': [
            'resources/Logo.png',
            'resources/SecondaryTile.png',
            'resources/SmallLogo.png',
            'resources/chrome.VisualElementsManifest.xml',
          ],
        },
      ],
    },
  ],
}
