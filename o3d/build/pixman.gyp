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
      'type': 'none',
      'conditions': [
        ['OS == "mac"',
          {
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
                  './configure --prefix=<(pkgconfigroot)/usr --disable-shared '
                      '&& '
                  # Build.
                  'make && '
                  # "Install" to pkgconfigroot.
                  'make install',
                ],
              },
            ],
          },
        ],
      ],
    },
  ],
}
