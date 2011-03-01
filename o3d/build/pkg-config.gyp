# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'pkgconfigbuilddir': '<(SHARED_INTERMEDIATE_DIR)/pkg-config',
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'pkg-config',
      'type': 'none',
      'conditions': [
        ['OS == "mac"',
          {
            'actions': [
              {
                'action_name': 'build_pkgconfig',
                'inputs': [
                  '<!@(find ../../<(pkgconfigdir) -name .svn -prune -o -type f '
                      '-print)',
                ],
                'outputs': [
                  '<(pkgconfigroot)/usr/bin/pkg-config',
                  # There are other outputs but it is sufficient to just list
                  # one.
                ],
                'action': [
                  'sh',
                  '-c',
                  # Delete existing build directory, if any.
                  'rm -rf <(pkgconfigbuilddir) && '
                  # Copy the pkg-config tree to the build directory.
                  'cp -r ../../<(pkgconfigdir) <(pkgconfigbuilddir) && '
                  # Go there!
                  'cd <(pkgconfigbuilddir) && '
                  # Configure it.
                  './configure --prefix=<(pkgconfigroot)/usr && '
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
