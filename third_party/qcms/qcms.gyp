# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'qcms',
      'product_name': 'qcms',
      'type': '<(library)',
      'sources': [
        'chain.c',
        'chain.h',
        'iccread.c',
        'matrix.c',
        'matrix.h',
        'qcms.h',
        'qcmsint.h',
        'qcmstypes.h',
        'transform.c',
        'transform-sse1.c',
        'transform-sse2.c',
        'transform_util.c',
        'transform_util.h',
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
