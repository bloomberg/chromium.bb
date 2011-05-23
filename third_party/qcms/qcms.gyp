# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'qcms',
      'product_name': 'qcms',
      'type': 'static_library',
      'sources': [
        'qcms.h',
        'qcmsint.h',
        'qcmstypes.h',
        'iccread.c',
        'transform-sse1.c',
        'transform-sse2.c',
        'transform.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['os_posix == 1 and OS != "mac" and (branding == "Chrome" or disable_sse2 == 1)', {
          'sources/': [
            ['exclude', 'transform-sse1.c'],
            ['exclude', 'transform-sse2.c'],
          ],
        },],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
