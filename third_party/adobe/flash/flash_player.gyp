# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # Always provide a target, so we can put the logic about whether there's
  # anything to be done in this file (instead of a higher-level .gyp file).
  'targets': [
    {
      'target_name': 'flash_player',
      'type': 'none',
      'conditions': [
        [ 'branding == "Chrome"', {
          'copies': [{
            'destination': '<(PRODUCT_DIR)',
            'files': [],
            'conditions': [
              [ 'OS == "linux" and target_arch == "ia32"', {
                'files': [
                  'binaries/linux/libgcflashplayer.so',
                  'binaries/linux/plugin.vch',
                ]
              }],
              [ 'OS == "mac"', {
                'files': [
                  'binaries/mac/Flash Player Plugin for Chrome.plugin',
                  'binaries/mac/plugin.vch',
                ]
              }],
              [ 'OS == "win"', {
                'files': [
                  'binaries/win/FlashPlayerCPLApp.cpl',
                  'binaries/win/gcswf32.dll',
                  'binaries/win/plugin.vch',
                  'symbols/win/gcswf32.pdb',
                ]
              }],
            ],
          }],
        }],
      ],
    },
    {
      'target_name': 'flapper_version_h',
      'type': 'none',
      'copies': [{
        'destination': '<(SHARED_INTERMEDIATE_DIR)',
        'files': [],
        'conditions': [
          # TODO(viettrungluu): actually bring in the headers.
          [ '1 == 1', {
            'files': [
              'flapper_version.h',  # The default, which indicates no Flapper.
            ],
          }],
        ],
      }],
    },
  ],
}
