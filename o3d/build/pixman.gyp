# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'pixmanbuilddir': '<(SHARED_INTERMEDIATE_DIR)/pixman',
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'pixman',
      'conditions': [
        ['OS == "mac"',
          {
            'type': 'none',
            'actions': [
              {
                'action_name': 'build_pixman',
                'inputs': [
                  '<!@(find ../../<(pixmandir) -name .svn -prune -o -type f '
                      '-print)',
                ],
                'outputs': [
                  '<(pkgconfigroot)/usr/lib/libpixman-1.a',
                  # There are other outputs but it is sufficient to just list
                  # one.
                ],
                'action': [
                  'sh',
                  '-c',
                  # Delete existing build directory, if any.
                  'rm -rf <(pixmanbuilddir) && '
                  # Copy the pixman tree to the build directory.
                  'cp -r ../../<(pixmandir) <(pixmanbuilddir) && '
                  # Go there!
                  'cd <(pixmanbuilddir) && '
                  # Configure it.
                  'CFLAGS="-arch <(mac_gcc_arch)" ./configure '
                      '--prefix=<(pkgconfigroot)/usr --disable-shared && '
                  # Build.
                  'make && '
                  # "Install" to pkgconfigroot.
                  'make install',
                ],
              },
            ],
          },
        ],
        ['OS == "win"',
          {
            'type': 'static_library',
            'defines': [
              'PACKAGE=pixman',
              'PACKAGE_VERSION=""',
              'PACKAGE_BUGREPORT=""',
              'USE_SSE2',
              'USE_MMX',
            ],
            'include_dirs': [
              '../../<(pixmandir)/pixman',
            ],
            'sources': [
              '../../<(pixmandir)/pixman/pixman.c',
              '../../<(pixmandir)/pixman/pixman-access.c',
              '../../<(pixmandir)/pixman/pixman-access-accessors.c',
              '../../<(pixmandir)/pixman/pixman-bits-image.c',
              '../../<(pixmandir)/pixman/pixman-combine32.c',
              '../../<(pixmandir)/pixman/pixman-combine64.c',
              '../../<(pixmandir)/pixman/pixman-conical-gradient.c',
              '../../<(pixmandir)/pixman/pixman-cpu.c',
              '../../<(pixmandir)/pixman/pixman-edge.c',
              '../../<(pixmandir)/pixman/pixman-edge-accessors.c',
              '../../<(pixmandir)/pixman/pixman-fast-path.c',
              '../../<(pixmandir)/pixman/pixman-general.c',
              '../../<(pixmandir)/pixman/pixman-gradient-walker.c',
              '../../<(pixmandir)/pixman/pixman-implementation.c',
              '../../<(pixmandir)/pixman/pixman-image.c',
              '../../<(pixmandir)/pixman/pixman-linear-gradient.c',
              '../../<(pixmandir)/pixman/pixman-matrix.c',
              '../../<(pixmandir)/pixman/pixman-mmx.c',
              '../../<(pixmandir)/pixman/pixman-radial-gradient.c',
              '../../<(pixmandir)/pixman/pixman-region16.c',
              '../../<(pixmandir)/pixman/pixman-region32.c',
              '../../<(pixmandir)/pixman/pixman-solid-fill.c',
              '../../<(pixmandir)/pixman/pixman-sse2.c',
              '../../<(pixmandir)/pixman/pixman-timer.c',
              '../../<(pixmandir)/pixman/pixman-trap.c',
              '../../<(pixmandir)/pixman/pixman-utils.c',
            ],
          },
        ],
      ],
    },
  ],
}
