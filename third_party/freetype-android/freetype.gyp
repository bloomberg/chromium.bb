# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    [ 'OS == "android"', {
      'targets': [
        {
          'target_name': 'ft2',
          'type': 'static_library',
          'toolsets': ['target'],
          'sources': [
            # The following files are not sorted alphabetically, but in the
            # same order as in Android.mk to ease maintenance.
            'src/src/base/ftbbox.c',
            'src/src/base/ftbitmap.c',
            'src/src/base/ftfstype.c',
            'src/src/base/ftglyph.c',
            'src/src/base/ftlcdfil.c',
            'src/src/base/ftstroke.c',
            'src/src/base/fttype1.c',
            'src/src/base/ftxf86.c',
            'src/src/base/ftbase.c',
            'src/src/base/ftsystem.c',
            'src/src/base/ftinit.c',
            'src/src/base/ftgasp.c',
            'src/src/base/ftmm.c',
            'src/src/gzip/ftgzip.c',
            'src/src/raster/raster.c',
            'src/src/sfnt/sfnt.c',
            'src/src/smooth/smooth.c',
            'src/src/autofit/autofit.c',
            'src/src/truetype/truetype.c',
            'src/src/cff/cff.c',
            'src/src/psnames/psnames.c',
            'src/src/pshinter/pshinter.c',
          ],
          'dependencies': [
            '../libpng/libpng.gyp:libpng',
            '../zlib/zlib.gyp:zlib',
          ],
          'include_dirs': [
            'src/include',
          ],
          'defines': [
            'FT2_BUILD_LIBRARY',
            'DARWIN_NO_CARBON',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '../../third_party/freetype-android/src/include',
            ],
          },
        },
      ],
    }],
  ],
}
