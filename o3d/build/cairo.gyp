# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'cairobuilddir': '<(SHARED_INTERMEDIATE_DIR)/cairo',
  },
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'cairo',
      'type': 'none',
      'conditions': [
        ['OS == "mac"',
          {
            'dependencies': [
              'pixman.gyp:pixman',
              'pkg-config.gyp:pkg-config',
            ],
            'actions': [
              {
                'action_name': 'build_cairo',
                'inputs': [
                  # Tragically, GYP/XCode has a limitation on the number of
                  # inputs that an action may have, and cairo's 3000+ files is
                  # over the limit, so we declare a dependency on only the first
                  # 2000 files. This is fine because any update to a newer
                  # version of cairo is likely to modify at least one of those
                  # files, which is sufficient to re-run the action.
                  '<!@(find ../../<(cairodir) -name .svn -prune -o -type f '
                      '-print | head -n 2000)',
                ],
                'inputs': [
                  # The cairo build won't actually read this file at all, but
                  # it does read the installed pixman headers, so if pixman has
                  # been rebuilt then we want cairo to be rebuilt too and it is
                  # simpler to just list this file than list all the pixman
                  # headers.
                  '<(pkgconfigroot)/usr/lib/libpixman-1.a',
                ],
                'outputs': [
                  '<(pkgconfigroot)/usr/lib/libcairo.a',
                  # There are other outputs but it is sufficient to just list
                  # one.
                ],
                'action': [
                  'sh',
                  '-c',
                  # Delete existing build directory, if any.
                  'rm -rf <(cairobuilddir) && '
                  # Copy the cairo tree to the build directory.
                  'cp -r ../../<(cairodir) <(cairobuilddir) && '
                  # Go there!
                  'cd <(cairobuilddir) && '
                  # Put our pkg-config binary into the PATH so cairo's build can
                  # find it.
                  'PATH=<(pkgconfigroot)/usr/bin:$PATH && '
                  # Configure it. We disable png and svg because we don't need
                  # them and they require additional external dependencies to
                  # build.
                  './configure --prefix=<(pkgconfigroot)/usr --disable-shared '
                      '--disable-png --disable-svg && '
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
