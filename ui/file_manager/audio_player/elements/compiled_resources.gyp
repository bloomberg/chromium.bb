# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'volume_controller',
      'variables': {
        'depends': [],
        'externs': []
      },
      'includes': [
        '../../compile_js.gypi'
      ]
    },
    {
      'target_name': 'track_list',
      'variables': {
        'depends': [],
        'externs': [
          '../../externs/es7_workaround.js'
        ]
      },
      'includes': [
        '../../compile_js.gypi'
      ]
    },
    {
      'target_name': 'control_panel',
      'variables': {
        'depends': [],
        'externs': []
      },
      'includes': [
        '../../compile_js.gypi'
      ]
    },
    {
      'target_name': 'audio_player',
      'variables': {
        'depends': [
          '../js/audio_player_model.js',
          'track_list.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_extensions.js',
          '../../externs/es7_workaround.js',
        ]
      },
      'includes': [
        '../../compile_js.gypi'
      ]
    }
  ],
}
