# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This file was split off from ppapi.gyp to prevent PPAPI users from
# needing to DEPS in ~10K files due to mesa.

{
  'includes': [
    '../../../third_party/mesa/mesa.gypi',
  ],
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'targets': [
    {
      'target_name': 'ppapi_egl',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_c',
      ],
      'include_dirs': [
        'include',
      ],
      'defines': [
        # Do not export internal Mesa funcations. Exporting them is not
        # required because we are compiling both - API dispatcher and driver
        # into a single library.
        'PUBLIC=',
        # Define a new PPAPI platform.
        '_EGL_PLATFORM_PPAPI=_EGL_NUM_PLATFORMS',
        '_EGL_NATIVE_PLATFORM=_EGL_PLATFORM_PPAPI',
      ],
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '_EGL_OS_WINDOWS',
          ],
        }],
        ['OS=="mac"', {
          # TODO(alokp): Make this compile on mac.
          'suppress_wildcard': 1,
        }],
      ],
      'sources': [
        # Mesa EGL API dispatcher sources.
        '<@(mesa_egl_sources)',
        # PPAPI EGL driver sources.
        'egl/egldriver.c',
        'egl/egldriver_ppapi.c',
      ],
    },
  ],
}
