# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'compositor',
      'type': '<(library)',
      'msvs_guid': '21CEE0E3-6F4E-4F01-B8C9-F7751CC21AA9',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/app/app.gyp:app_base',
        '<(DEPTH)/ui/ui.gyp:ui_gfx',
      ],
      'sources': [
        'compositor.cc',
        'compositor_gl.cc',
        'compositor.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources!': [
            'compositor.cc',
          ],
        }, {
          'sources!': [
            'compositor_gl.cc',
          ]
        }],
      ],
    },
  ],
}