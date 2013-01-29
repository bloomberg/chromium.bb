# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'rebootondisconnect',
      'type': 'none',
      'dependencies': [
        'rebootondisconnect_symbols',
      ],
      'actions': [
        {
          'action_name': 'strip_rebootondisconnect',
          'inputs': ['<(PRODUCT_DIR)/rebootondisconnect_symbols'],
          'outputs': ['<(PRODUCT_DIR)/rebootondisconnect'],
          'action': [
            '<!(/bin/echo -n $STRIP)',
            '--strip-unneeded',
            '<@(_inputs)',
            '-o',
            '<@(_outputs)',
          ],
        },
      ],
    }, {
      'target_name': 'rebootondisconnect_symbols',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../common/common.gyp:android_tools_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'rebootondisconnect.cc',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
