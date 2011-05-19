# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'iccjpeg',
      'product_name': 'iccjpeg',
      'type': 'static_library',
      'dependencies': [
        '<(libjpeg_gyp_path):libjpeg',
      ],
      'sources': [
        'iccjpeg.c',
        'iccjpeg.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
