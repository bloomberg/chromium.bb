# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    [ 'OS == "win"', {
      'targets': [
        {
          'target_name': 'flash_player',
          'type': 'none',
          'conditions': [
            ['branding == "Chrome"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    'binaries/win/gcswf32.dll',
                  ],
                },
              ],
            }],
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
