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
