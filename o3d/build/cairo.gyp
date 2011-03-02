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
      'conditions': [
        ['OS == "mac"',
          {
            'type': 'none',
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
        ['OS == "win"',
          {
            'type': 'static_library',
            'defines': [
              'CAIRO_WIN32_STATIC_BUILD',
              # TODO(gangji): This may hurt perf. Investigate.
              'DISABLE_SOME_FLOATING_POINT',
            ],
            'include_dirs': [
              '../../<(cairodir)/src',
              '../../<(pngdir)/',
              '../../<(pixmandir)/pixman',
              '../../<(zlibdir)/',
              'misc'
            ],
            'sources': [
              '../../<(cairodir)/src/cairo-analysis-surface.c',
              '../../<(cairodir)/src/cairo-arc.c',
              '../../<(cairodir)/src/cairo-array.c',
              '../../<(cairodir)/src/cairo-atomic.c',
              '../../<(cairodir)/src/cairo-base64-stream.c',
              '../../<(cairodir)/src/cairo-base85-stream.c',
              '../../<(cairodir)/src/cairo-bentley-ottmann.c',
              '../../<(cairodir)/src/cairo-bentley-ottmann-rectangular.c',
              '../../<(cairodir)/src/cairo-bentley-ottmann-rectilinear.c',
              '../../<(cairodir)/src/cairo-botor-scan-converter.c',
              '../../<(cairodir)/src/cairo-boxes.c',
              '../../<(cairodir)/src/cairo.c',
              '../../<(cairodir)/src/cairo-cache.c',
              '../../<(cairodir)/src/cairo-clip.c',
              '../../<(cairodir)/src/cairo-color.c',
              '../../<(cairodir)/src/cairo-composite-rectangles.c',
              '../../<(cairodir)/src/cairo-debug.c',
              '../../<(cairodir)/src/cairo-device.c',
              '../../<(cairodir)/src/cairo-fixed.c',
              '../../<(cairodir)/src/cairo-font-face.c',
              '../../<(cairodir)/src/cairo-font-face-twin.c',
              '../../<(cairodir)/src/cairo-font-face-twin-data.c',
              '../../<(cairodir)/src/cairo-font-options.c',
              '../../<(cairodir)/src/cairo-freelist.c',
              '../../<(cairodir)/src/cairo-freed-pool.c',
              '../../<(cairodir)/src/cairo-gstate.c',
              '../../<(cairodir)/src/cairo-hash.c',
              '../../<(cairodir)/src/cairo-hull.c',
              '../../<(cairodir)/src/cairo-image-info.c',
              '../../<(cairodir)/src/cairo-image-surface.c',
              '../../<(cairodir)/src/cairo-lzw.c',
              '../../<(cairodir)/src/cairo-matrix.c',
              '../../<(cairodir)/src/cairo-recording-surface.c',
              '../../<(cairodir)/src/cairo-misc.c',
              '../../<(cairodir)/src/cairo-mutex.c',
              '../../<(cairodir)/src/cairo-observer.c',
              '../../<(cairodir)/src/cairo-output-stream.c',
              '../../<(cairodir)/src/cairo-paginated-surface.c',
              '../../<(cairodir)/src/cairo-path-bounds.c',
              '../../<(cairodir)/src/cairo-path.c',
              '../../<(cairodir)/src/cairo-path-fill.c',
              '../../<(cairodir)/src/cairo-path-fixed.c',
              '../../<(cairodir)/src/cairo-path-in-fill.c',
              '../../<(cairodir)/src/cairo-path-stroke.c',
              '../../<(cairodir)/src/cairo-pattern.c',
              '../../<(cairodir)/src/cairo-pen.c',
              '../../<(cairodir)/src/cairo-png.c',
              '../../<(cairodir)/src/cairo-polygon.c',
              '../../<(cairodir)/src/cairo-rectangle.c',
              '../../<(cairodir)/src/cairo-rectangular-scan-converter.c',
              '../../<(cairodir)/src/cairo-region.c',
              '../../<(cairodir)/src/cairo-rtree.c',
              '../../<(cairodir)/src/cairo-scaled-font.c',
              '../../<(cairodir)/src/cairo-scaled-font-subsets.c',
              '../../<(cairodir)/src/cairo-slope.c',
              '../../<(cairodir)/src/cairo-spans.c',
              '../../<(cairodir)/src/cairo-spline.c',
              '../../<(cairodir)/src/cairo-stroke-style.c',
              '../../<(cairodir)/src/cairo-surface.c',
              '../../<(cairodir)/src/cairo-surface-fallback.c',
              '../../<(cairodir)/src/cairo-surface-clipper.c',
              '../../<(cairodir)/src/cairo-surface-offset.c',
              '../../<(cairodir)/src/cairo-surface-snapshot.c',
              '../../<(cairodir)/src/cairo-surface-subsurface.c',
              '../../<(cairodir)/src/cairo-surface-wrapper.c',
              '../../<(cairodir)/src/cairo-system.c',
              '../../<(cairodir)/src/cairo-tor-scan-converter.c',
              '../../<(cairodir)/src/cairo-toy-font-face.c',
              '../../<(cairodir)/src/cairo-traps.c',
              '../../<(cairodir)/src/cairo-truetype-subset.c',
              '../../<(cairodir)/src/cairo-unicode.c',
              '../../<(cairodir)/src/cairo-user-font.c',
              '../../<(cairodir)/src/cairo-version.c',
              '../../<(cairodir)/src/cairo-wideint.c',
              '../../<(cairodir)/src/cairo-win32-surface.c',
              '../../<(cairodir)/src/cairo-win32-printing-surface.c',
              '../../<(cairodir)/src/cairo-win32-font.c',
            ],
          },        
        ],
      ],
    },
  ],
}
