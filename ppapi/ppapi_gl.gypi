# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This file was split off from ppapi.gyp to prevent PPAPI users from
# needing to DEPS in ~10K files due to mesa.
{
  'includes': [
    '../third_party/mesa/mesa.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_egl',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_c',
      ],
      'include_dirs': [
        'lib/gl/include',
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
        'lib/gl/egl/egldriver.c',
        'lib/gl/egl/egldriver_ppapi.c',
      ],
    },
    {
      'target_name': 'ppapi_gles2',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
      ],
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        'lib/gl/gles2/gl2ext_ppapi.c',
        'lib/gl/gles2/gl2ext_ppapi.h',
        'lib/gl/gles2/gles2.c',
      ],
    },
    {
      'target_name': 'ppapi_gles_bindings',
      'type': 'none',
      'suppress_wildcard': 1,
      'actions': [
       {
         'action_name': 'generate_ppapi_gles_bindings',
         'variables': {
           'gles_script': '<(DEPTH)/gpu/command_buffer/build_gles2_cmd_buffer.py',
         },
         'inputs': [
           '<(gles_script)',
         ],
         'outputs': [
           'c/dev/ppb_opengles_dev.h',
           'lib/gl/gles2/gles2.c',
         ],
         'action': [
           'python',
           '<(gles_script)',
           '--alternate-mode=ppapi'
         ],
         'message': 'Generating Pepper OpenGL ES bindings',
       }],
    },
  ],
}
