# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
              [ 'chromeos == 1', {
                'files': [
                  'binaries/chromeos/libgcflashplayer.so',
                  'binaries/linux/plugin.vch',
                ]
              }],
              [ 'OS == "linux" and target_arch == "ia32" and chromeos == 0', {
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
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
