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
        'src/chain.c',
        'src/chain.h',
        'src/iccread.c',
        'src/matrix.c',
        'src/matrix.h',
        'src/qcms.h',
        'src/qcmsint.h',
        'src/qcmstypes.h',
        'src/transform.c',
        'src/transform_util.c',
        'src/transform_util.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          './src',
        ],
      },
      'conditions': [
        [ 'target_arch != "arm" and OS in ["linux", "freebsd", "openbsd", "solaris"]', {
          'cflags': [
            '-msse',
            '-msse2',
          ],
        }],
        [ 'target_arch != "arm"', {
          'sources': [
            'src/transform-sse1.c',
            'src/transform-sse2.c',
          ],
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
