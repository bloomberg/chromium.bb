# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'ozone_platform_caca%': 0,
  },
  'conditions': [
    ['<(ozone_platform_caca) == 1', {
      'variables': {
        'ozone_platforms': [
          'caca'
        ],
      },
      'link_settings': {
        'libraries': [
          '-lcaca',
        ],
      },
      'sources': [
        'caca_connection.cc',
        'caca_connection.h',
        'ozone_platform_caca.cc',
        'ozone_platform_caca.h',
        'caca_event_factory.cc',
        'caca_event_factory.h',
        'caca_surface_factory.cc',
        'caca_surface_factory.h',
      ],
    }],
  ],
}
